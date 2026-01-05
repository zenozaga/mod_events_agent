#include "status.h"
#include "core.h"
#include <cjson/cJSON.h>

static event_driver_t *g_driver = NULL;

void handle_status(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    extern mod_event_agent_globals_t globals;
    command_stats_increment_received();

    switch_bool_t success = SWITCH_TRUE;
    const char *message = "System status";
    switch_bool_t log_level_updated = SWITCH_FALSE;

    if (data && len > 0) {
        cJSON *request = cJSON_Parse(data);
        if (!request) {
            success = SWITCH_FALSE;
            message = "Invalid JSON payload";
        } else {
            cJSON *level_item = cJSON_GetObjectItemCaseSensitive(request, "log_level");
            if (!level_item) {
                level_item = cJSON_GetObjectItemCaseSensitive(request, "log-level");
            }

            if (level_item) {
                if (!cJSON_IsString(level_item) || !level_item->valuestring || !*level_item->valuestring) {
                    success = SWITCH_FALSE;
                    message = "log_level must be a non-empty string";
                } else {
                    switch_log_level_t lvl = switch_log_str2level(level_item->valuestring);
                    if (lvl == SWITCH_LOG_INVALID) {
                        success = SWITCH_FALSE;
                        message = "Invalid log level value";
                    } else {
                        globals.log_level = lvl;
                        log_level_updated = SWITCH_TRUE;
                        message = "Log level updated";
                        EVENT_LOG_INFO("Runtime log level set to %s via %s",
                                       switch_log_level2str(globals.log_level),
                                       subject ? subject : "unknown");
                    }
                }
            }

            cJSON_Delete(request);
        }
    }

    uint64_t requests_received = 0;
    uint64_t requests_success = 0;
    uint64_t requests_failed = 0;
    command_stats_get(&requests_received, &requests_success, &requests_failed);

    cJSON *response = build_json_response_object(success, message);
    if (!response) {
        EVENT_LOG_ERROR("Failed to build status response JSON");
        command_stats_increment_failed();
        return;
    }

    cJSON *data_obj = cJSON_CreateObject();
    if (data_obj) {
        cJSON_AddStringToObject(data_obj, "version", MOD_EVENT_AGENT_VERSION);
        cJSON *stats = cJSON_CreateObject();
        if (stats) {
            cJSON_AddNumberToObject(stats, "requests_received", (double)requests_received);
            cJSON_AddNumberToObject(stats, "requests_success", (double)requests_success);
            cJSON_AddNumberToObject(stats, "requests_failed", (double)requests_failed);
            cJSON_AddItemToObject(data_obj, "stats", stats);
        }

        cJSON_AddStringToObject(data_obj, "log_level", switch_log_level2str(globals.log_level));
        if (log_level_updated) {
            cJSON_AddBoolToObject(data_obj, "log_level_updated", SWITCH_TRUE);
        }

        cJSON_AddItemToObject(response, "data", data_obj);
    }

    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    if (!response_str) {
        EVENT_LOG_ERROR("Failed to serialize status response JSON");
        command_stats_increment_failed();
        return;
    }

    if (reply_to) {
        g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
    }

    free(response_str);

    if (success) {
        command_stats_increment_success();
    } else {
        command_stats_increment_failed();
    }
}

switch_status_t command_status_register(event_driver_t *driver) {
    extern mod_event_agent_globals_t globals;
    g_driver = driver;
    
    if (driver->subscribe(driver, "freeswitch.cmd.status", handle_status, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.cmd.status");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.cmd.status (broadcast)");
    
    char direct_subject[256];
    snprintf(direct_subject, sizeof(direct_subject), "freeswitch.cmd.status.%s", globals.node_id);
    if (driver->subscribe(driver, direct_subject, handle_status, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to %s", direct_subject);
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: %s (direct)", direct_subject);
    
    return SWITCH_STATUS_SUCCESS;
}
