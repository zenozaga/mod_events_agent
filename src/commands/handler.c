#include "../mod_event_agent.h"
#include "../dialplan/commands.h"
#include "core.h"
#include "call.h"
#include "api.h"
#include "status.h"
#include <string.h>

static event_driver_t *g_driver = NULL;
static switch_hash_t *g_registry = NULL;
static command_handler_fn g_default_handler = NULL;
static char g_subject_api[256] = {0};
static char g_subject_node[256] = {0};
static switch_bool_t g_node_subscription = SWITCH_FALSE;

static const char *commands_prefix(void) {
    extern mod_event_agent_globals_t globals;
    return (globals.subject_prefix && *globals.subject_prefix) ? globals.subject_prefix : DEFAULT_SUBJECT_PREFIX;
}

static void publish_response(const char *reply_to, switch_bool_t success, const char *message, cJSON *data) {
    if (!reply_to || !g_driver) {
        if (data) {
            cJSON_Delete(data);
        }
        return;
    }

    cJSON *response = build_json_response_object(success, message ? message : (success ? "Command executed" : "Command failed"));
    if (!response) {
        if (data) {
            cJSON_Delete(data);
        }
        return;
    }

    if (data) {
        cJSON_AddItemToObject(response, "data", data);
        data = NULL;
    }

    char *json_str = cJSON_PrintUnformatted(response);
    if (json_str) {
        g_driver->publish(g_driver, reply_to, json_str, strlen(json_str));
    }
    switch_safe_free(json_str);
    cJSON_Delete(response);

    if (data) {
        cJSON_Delete(data);
    }
}

static command_handler_fn lookup_handler(const char *name) {
    if (!g_registry || switch_strlen_zero(name)) {
        return NULL;
    }
    return (command_handler_fn)switch_core_hash_find(g_registry, name);
}

switch_status_t command_register_handler(const char *name, command_handler_fn handler) {
    if (!g_registry || switch_strlen_zero(name) || !handler) {
        return SWITCH_STATUS_FALSE;
    }

    switch_core_hash_insert(g_registry, name, handler);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Registered command handler: %s", name);
    return SWITCH_STATUS_SUCCESS;
}

void command_register_default_handler(command_handler_fn handler) {
    g_default_handler = handler;
}

static void dispatch_command(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    command_stats_increment_received();

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] Invalid JSON payload on subject %s", subject ? subject : "<unknown>");
        command_stats_increment_failed();
        publish_response(reply_to, SWITCH_FALSE, "Invalid JSON payload", NULL);
        return;
    }

    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return;
    }

    cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(json, "command");
    if (!cmd_item || !cJSON_IsString(cmd_item) || switch_strlen_zero(cmd_item->valuestring)) {
        cJSON_Delete(json);
        command_stats_increment_failed();
        publish_response(reply_to, SWITCH_FALSE, "Missing 'command' string", NULL);
        return;
    }

    switch_bool_t async = SWITCH_FALSE;
    cJSON *async_item = cJSON_GetObjectItemCaseSensitive(json, "async");
    if (async_item) {
        async = cJSON_IsTrue(async_item) ? SWITCH_TRUE : SWITCH_FALSE;
    }

    const char *command_name = cmd_item->valuestring;
    switch_log_printf(SWITCH_CHANNEL_LOG,
                      SWITCH_LOG_DEBUG,
                      "[mod_event_agent] Received command %s via %s (reply=%s, async=%s)",
                      command_name,
                      subject ? subject : "<unknown>",
                      reply_to ? reply_to : "<none>",
                      async == SWITCH_TRUE ? "true" : "false");

    command_handler_fn handler = lookup_handler(command_name);
    if (!handler) {
        handler = g_default_handler;
    }

    if (!handler) {
        cJSON_Delete(json);
        command_stats_increment_failed();
        publish_response(reply_to, SWITCH_FALSE, "Unknown command", NULL);
        return;
    }

    command_request_t request = {
        .payload = json,
        .command = command_name,
        .subject = subject,
        .reply_to = reply_to,
        .async = async
    };

    command_result_t result = handler(&request);
    const switch_bool_t success = result.error == NULL;

    if (success) {
        command_stats_increment_success();
    } else {
        command_stats_increment_failed();
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Command %s failed: %s", command_name, result.error);
    }

    if (!async) {
        if (!success && !result.message && result.error) {
            result.message = result.error;
        }
        publish_response(reply_to, success, result.message, result.data);
        result.data = NULL;
    } else if (result.data) {
        cJSON_Delete(result.data);
        result.data = NULL;
    }

    command_result_free(&result);
    cJSON_Delete(json);
}

switch_status_t command_handler_init(event_driver_t *driver, switch_memory_pool_t *pool, dialplan_manager_t *dialplan_manager) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Initializing command handler");

    g_driver = driver;

    (void)pool;

    if (switch_core_hash_init(&g_registry) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to allocate command registry");
        return SWITCH_STATUS_FALSE;
    }

    if (command_api_register() != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to register default API handler");
        return SWITCH_STATUS_FALSE;
    }
    if (command_call_register() != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to register call commands");
        return SWITCH_STATUS_FALSE;
    }
    if (command_status_register() != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to register status command");
        return SWITCH_STATUS_FALSE;
    }
    if (dialplan_manager && command_dialplan_init(dialplan_manager) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] Dialplan commands could not be registered (continuing)");
    }

    const char *prefix = commands_prefix();
    switch_snprintf(g_subject_api, sizeof(g_subject_api), "%s.api", prefix);
    if (driver->subscribe(driver, g_subject_api, dispatch_command, NULL) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to subscribe to %s", g_subject_api);
        return SWITCH_STATUS_FALSE;
    }

    extern mod_event_agent_globals_t globals;
    if (globals.node_id && *globals.node_id) {
        switch_snprintf(g_subject_node, sizeof(g_subject_node), "%s.node.%s", prefix, globals.node_id);
        if (driver->subscribe(driver, g_subject_node, dispatch_command, NULL) == SWITCH_STATUS_SUCCESS) {
            g_node_subscription = SWITCH_TRUE;
        } else {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] Failed to subscribe to %s", g_subject_node);
            g_subject_node[0] = '\0';
        }
    }

    switch_log_printf(SWITCH_CHANNEL_LOG,
                      SWITCH_LOG_INFO,
                      "[mod_event_agent] Command handler ready on %s (broadcast)%s",
                      g_subject_api,
                      g_node_subscription ? " + direct node channel" : "");

    return SWITCH_STATUS_SUCCESS;
}

void command_handler_shutdown(void) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Shutting down command handler");

    if (g_driver) {
        if (*g_subject_api) {
            g_driver->unsubscribe(g_driver, g_subject_api);
        }
        if (g_node_subscription && *g_subject_node) {
            g_driver->unsubscribe(g_driver, g_subject_node);
        }
    }

    g_driver = NULL;
    g_registry = NULL;
    g_default_handler = NULL;
    g_subject_api[0] = '\0';
    g_subject_node[0] = '\0';
    g_node_subscription = SWITCH_FALSE;

    command_dialplan_shutdown();

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Command handler shutdown complete");
}

void command_handler_get_stats(uint64_t *requests, uint64_t *success, uint64_t *failed) {
    command_stats_get(requests, success, failed);
}
