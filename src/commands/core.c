#include "core.h"
#include <cjson/cJSON.h>

#define COMMAND_DEFAULT_SUCCESS_MSG "Command executed"

static command_stats_t g_stats = {0};

uint64_t command_current_timestamp_us(void) {
    return (uint64_t)switch_time_now();
}

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

cJSON* build_json_response_object(switch_bool_t success, const char *message) {
    extern mod_event_agent_globals_t globals;
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return NULL;
    }

    cJSON_AddBoolToObject(json, "success", success);
    cJSON_AddStringToObject(json, "status", success ? "success" : "error");
    cJSON_AddStringToObject(json, "message", message ? message : "");
    cJSON_AddNumberToObject(json, "timestamp", (double)command_current_timestamp_us());

    if (globals.node_id && strlen(globals.node_id) > 0) {
        cJSON_AddStringToObject(json, "node_id", globals.node_id);
    } else {
        cJSON_AddStringToObject(json, "node_id", "unknown");
    }

    return json;
}

char* build_json_response(switch_bool_t success, const char *message, const char *data) {
    cJSON *json = build_json_response_object(success, message);
    if (!json) {
        return NULL;
    }

    if (data) {
        cJSON_AddStringToObject(json, "data", data);
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

command_result_t command_result_ok(void) {
    command_result_t result = {0};
    result.message = COMMAND_DEFAULT_SUCCESS_MSG;
    return result;
}

command_result_t command_result_from_string(const char *value) {
    command_result_t result = command_result_ok();
    if (value) {
        result.data = cJSON_CreateString(value);
    }
    return result;
}

command_result_t command_result_error(const char *message) {
    command_result_t result = {0};
    const char *fallback = message ? message : "Unknown error";
    result.error = switch_safe_strdup(fallback);
    result.message = result.error;
    return result;
}

void command_result_free(command_result_t *result) {
    if (!result) {
        return;
    }
    if (result->data) {
        cJSON_Delete(result->data);
        result->data = NULL;
    }
    switch_safe_free(result->error);
    result->message = NULL;
}
