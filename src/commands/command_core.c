#include "command_core.h"
#include <cjson/cJSON.h>

static command_stats_t g_stats = {0};

switch_bool_t should_process_request(cJSON *json) {
    extern mod_event_agent_globals_t globals;
    
    cJSON *node_id_item = cJSON_GetObjectItem(json, "node_id");
    
    if (!node_id_item || !cJSON_IsString(node_id_item)) {
        return SWITCH_TRUE;
    }
    
    const char *target_node = node_id_item->valuestring;
    
    if (globals.node_id && strcmp(target_node, globals.node_id) == 0) {
        return SWITCH_TRUE;
    }
    
    EVENT_LOG_DEBUG("Skipping request - target node: %s, our node: %s", 
                    target_node, globals.node_id ? globals.node_id : "unknown");
    return SWITCH_FALSE;
}

char* build_json_response(switch_bool_t success, const char *message, const char *data) {
    extern mod_event_agent_globals_t globals;
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "success", success);
    cJSON_AddStringToObject(json, "message", message ? message : "");
    if (data) cJSON_AddStringToObject(json, "data", data);
    cJSON_AddNumberToObject(json, "timestamp", (double)switch_time_now());
    
    if (globals.node_id && strlen(globals.node_id) > 0) {
        cJSON_AddStringToObject(json, "node_id", globals.node_id);
    } else {
        cJSON_AddStringToObject(json, "node_id", "unknown");
    }
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return json_str;
}

void command_stats_increment_received(void) {
    g_stats.requests_received++;
}

void command_stats_increment_success(void) {
    g_stats.requests_success++;
}

void command_stats_increment_failed(void) {
    g_stats.requests_failed++;
}

void command_stats_get(uint64_t *requests, uint64_t *success, uint64_t *failed) {
    if (requests) *requests = g_stats.requests_received;
    if (success) *success = g_stats.requests_success;
    if (failed) *failed = g_stats.requests_failed;
}
