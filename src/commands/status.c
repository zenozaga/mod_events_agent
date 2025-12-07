#include "status.h"
#include "core.h"
#include <cjson/cJSON.h>

static event_driver_t *g_driver = NULL;

void handle_status(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    command_stats_increment_received();
    
    uint64_t requests_received = 0;
    uint64_t requests_success = 0;
    uint64_t requests_failed = 0;
    command_stats_get(&requests_received, &requests_success, &requests_failed);
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "success", SWITCH_TRUE);
    cJSON_AddStringToObject(json, "message", "System status");
    
    cJSON *stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "requests_received", (double)requests_received);
    cJSON_AddNumberToObject(stats, "requests_success", (double)requests_success);
    cJSON_AddNumberToObject(stats, "requests_failed", (double)requests_failed);
    cJSON_AddItemToObject(json, "stats", stats);
    
    char *response_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
    
    free(response_str);
    command_stats_increment_success();
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
