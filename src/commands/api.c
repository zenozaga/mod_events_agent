#include "api.h"
#include "core.h"
#include <cjson/cJSON.h>

static event_driver_t *g_driver = NULL;

void handle_api_generic(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    command_stats_increment_received();
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        EVENT_LOG_ERROR("Invalid JSON in generic API request");
        char *error_response = build_json_response(SWITCH_FALSE, "Invalid JSON payload", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, error_response, strlen(error_response));
        free(error_response);
        command_stats_increment_failed();
        return;
    }
    
    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return;
    }
    
    cJSON *cmd_item = cJSON_GetObjectItem(json, "command");
    cJSON *args_item = cJSON_GetObjectItem(json, "args");
    
    if (!cmd_item || !cJSON_IsString(cmd_item)) {
        EVENT_LOG_ERROR("Missing or invalid 'command' field");
        char *error_response = build_json_response(SWITCH_FALSE, "Missing or invalid 'command' field", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, error_response, strlen(error_response));
        free(error_response);
        cJSON_Delete(json);
        command_stats_increment_failed();
        return;
    }
    
    const char *command = cmd_item->valuestring;
    const char *args = (args_item && cJSON_IsString(args_item)) ? args_item->valuestring : NULL;
    
    EVENT_LOG_INFO("📡 Executing API command: %s %s", command, args ? args : "(no args)");
    
    switch_stream_handle_t stream = { 0 };
    SWITCH_STANDARD_STREAM(stream);
    switch_status_t status = switch_api_execute(command, args, NULL, &stream);
    
    switch_bool_t has_error = SWITCH_FALSE;
    if (stream.data && strstr((char*)stream.data, "-ERR")) {
        has_error = SWITCH_TRUE;
    }
    
    char *response_str = NULL;
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        response_str = build_json_response(SWITCH_TRUE, "API command executed", stream.data ? (char*)stream.data : "");
        command_stats_increment_success();
    } else {
        const char *error_msg = stream.data ? (char*)stream.data : "Unknown error";
        response_str = build_json_response(SWITCH_FALSE, error_msg, NULL);
        command_stats_increment_failed();
    }
    
    if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
    
    free(response_str);
    switch_safe_free(stream.data);
    cJSON_Delete(json);
}

switch_status_t command_api_register(event_driver_t *driver) {
    extern mod_event_agent_globals_t globals;
    g_driver = driver;
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "🔌 Subscribing to: freeswitch.api (broadcast)\n");
    if (driver->subscribe(driver, "freeswitch.api", handle_api_generic, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.api");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.api (broadcast with JSON filtering)");
    
    char direct_subject[256];
    snprintf(direct_subject, sizeof(direct_subject), "freeswitch.api.%s", globals.node_id);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "🔌 Subscribing to: %s (direct)\n", direct_subject);
    if (driver->subscribe(driver, direct_subject, handle_api_generic, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to %s", direct_subject);
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: %s (direct targeting)", direct_subject);
    
    return SWITCH_STATUS_SUCCESS;
}
