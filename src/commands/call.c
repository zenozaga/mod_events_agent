#include "call.h"
#include "core.h"
#include <cjson/cJSON.h>

static event_driver_t *g_driver = NULL;

void handle_originate(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    char *response_str = NULL;
    
    command_stats_increment_received();
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        response_str = build_json_response(SWITCH_FALSE, "Invalid JSON", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
        free(response_str);
        command_stats_increment_failed();
        return;
    }
    
    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return;
    }
    
    cJSON *endpoint_json = cJSON_GetObjectItem(json, "endpoint");
    cJSON *extension_json = cJSON_GetObjectItem(json, "extension");
    
    if (!endpoint_json || !extension_json) {
        response_str = build_json_response(SWITCH_FALSE, "Missing required fields", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
        free(response_str);
        cJSON_Delete(json);
        command_stats_increment_failed();
        return;
    }
    
    const char *endpoint = endpoint_json->valuestring;
    const char *extension = extension_json->valuestring;
    cJSON *context_json = cJSON_GetObjectItem(json, "context");
    const char *context = context_json ? context_json->valuestring : "default";
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s %s", endpoint, extension, context);
    
    switch_stream_handle_t stream = { 0 };
    SWITCH_STANDARD_STREAM(stream);
    
    switch_status_t status = switch_api_execute("originate", cmd, NULL, &stream);
    
    switch_bool_t has_error = SWITCH_FALSE;
    if (stream.data && strncmp((char*)stream.data, "-ERR", 4) == 0) {
        has_error = SWITCH_TRUE;
    }
    
    if (status == SWITCH_STATUS_SUCCESS && stream.data && !has_error) {
        response_str = build_json_response(SWITCH_TRUE, "Call originated successfully", (char*)stream.data);
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

void handle_hangup(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    char *response_str = NULL;
    
    command_stats_increment_received();
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        response_str = build_json_response(SWITCH_FALSE, "Invalid JSON", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
        free(response_str);
        command_stats_increment_failed();
        return;
    }
    
    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return;
    }
    
    cJSON *uuid_json = cJSON_GetObjectItem(json, "uuid");
    if (!uuid_json) {
        response_str = build_json_response(SWITCH_FALSE, "Missing uuid", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
        free(response_str);
        cJSON_Delete(json);
        command_stats_increment_failed();
        return;
    }
    
    const char *uuid = uuid_json->valuestring;
    cJSON *cause_json = cJSON_GetObjectItem(json, "cause");
    const char *cause = cause_json ? cause_json->valuestring : "NORMAL_CLEARING";
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s %s", uuid, cause);
    
    switch_stream_handle_t stream = { 0 };
    SWITCH_STANDARD_STREAM(stream);
    
    switch_status_t status = switch_api_execute("uuid_kill", cmd, NULL, &stream);
    
    switch_bool_t has_error = SWITCH_FALSE;
    if (stream.data && strncmp((char*)stream.data, "-ERR", 4) == 0) {
        has_error = SWITCH_TRUE;
    }
    
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        response_str = build_json_response(SWITCH_TRUE, "Channel hangup successful", uuid);
        command_stats_increment_success();
    } else {
        const char *error_msg = stream.data ? (char*)stream.data : "Unknown error";
        response_str = build_json_response(SWITCH_FALSE, error_msg, uuid);
        command_stats_increment_failed();
    }
    
    if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
    
    free(response_str);
    switch_safe_free(stream.data);
    cJSON_Delete(json);
}

void handle_originate_async(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    command_stats_increment_received();
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        EVENT_LOG_ERROR("Invalid JSON in async originate");
        command_stats_increment_failed();
        return;
    }
    
    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return;
    }
    
    cJSON *endpoint_json = cJSON_GetObjectItem(json, "endpoint");
    cJSON *extension_json = cJSON_GetObjectItem(json, "extension");
    
    if (!endpoint_json || !extension_json) {
        EVENT_LOG_ERROR("Missing required fields in async originate");
        cJSON_Delete(json);
        command_stats_increment_failed();
        return;
    }
    
    const char *endpoint = endpoint_json->valuestring;
    const char *extension = extension_json->valuestring;
    cJSON *context_json = cJSON_GetObjectItem(json, "context");
    const char *context = context_json ? context_json->valuestring : "default";
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s %s", endpoint, extension, context);
    
    switch_stream_handle_t stream = { 0 };
    SWITCH_STANDARD_STREAM(stream);
    
    switch_status_t status = switch_api_execute("originate", cmd, NULL, &stream);
    
    switch_bool_t has_error = SWITCH_FALSE;
    if (stream.data && strncmp((char*)stream.data, "-ERR", 4) == 0) {
        has_error = SWITCH_TRUE;
    }
    
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        command_stats_increment_success();
    } else {
        command_stats_increment_failed();
        if (stream.data) {
            EVENT_LOG_ERROR((char*)stream.data);
        }
    }
    
    switch_safe_free(stream.data);
    cJSON_Delete(json);
}

void handle_hangup_async(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    command_stats_increment_received();
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        EVENT_LOG_ERROR("Invalid JSON in async hangup");
        command_stats_increment_failed();
        return;
    }
    
    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return;
    }
    
    cJSON *uuid_json = cJSON_GetObjectItem(json, "uuid");
    if (!uuid_json) {
        EVENT_LOG_ERROR("Missing uuid in async hangup");
        cJSON_Delete(json);
        command_stats_increment_failed();
        return;
    }
    
    const char *uuid = uuid_json->valuestring;
    cJSON *cause_json = cJSON_GetObjectItem(json, "cause");
    const char *cause = cause_json ? cause_json->valuestring : "NORMAL_CLEARING";
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s %s", uuid, cause);
    
    switch_stream_handle_t stream = { 0 };
    SWITCH_STANDARD_STREAM(stream);
    
    switch_status_t status = switch_api_execute("uuid_kill", cmd, NULL, &stream);
    
    switch_bool_t has_error = SWITCH_FALSE;
    if (stream.data && strncmp((char*)stream.data, "-ERR", 4) == 0) {
        has_error = SWITCH_TRUE;
    }
    
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        command_stats_increment_success();
    } else {
        command_stats_increment_failed();
        if (stream.data) {
            EVENT_LOG_ERROR((char*)stream.data);
        }
    }
    
    switch_safe_free(stream.data);
    cJSON_Delete(json);
}

switch_status_t command_call_register(event_driver_t *driver) {
    extern mod_event_agent_globals_t globals;
    g_driver = driver;
    char direct_subject[256];
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "🔌 Subscribing to: freeswitch.cmd.originate (broadcast)\n");
    if (driver->subscribe(driver, "freeswitch.cmd.originate", handle_originate, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.cmd.originate");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.cmd.originate (broadcast)");
    
    snprintf(direct_subject, sizeof(direct_subject), "freeswitch.cmd.originate.%s", globals.node_id);
    if (driver->subscribe(driver, direct_subject, handle_originate, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to %s", direct_subject);
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: %s (direct)", direct_subject);
    
    if (driver->subscribe(driver, "freeswitch.cmd.hangup", handle_hangup, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.cmd.hangup");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.cmd.hangup (broadcast)");
    
    snprintf(direct_subject, sizeof(direct_subject), "freeswitch.cmd.hangup.%s", globals.node_id);
    if (driver->subscribe(driver, direct_subject, handle_hangup, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to %s", direct_subject);
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: %s (direct)", direct_subject);
    
    if (driver->subscribe(driver, "freeswitch.cmd.async.originate", handle_originate_async, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.cmd.async.originate");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.cmd.async.originate (broadcast async)");
    
    snprintf(direct_subject, sizeof(direct_subject), "freeswitch.cmd.async.originate.%s", globals.node_id);
    if (driver->subscribe(driver, direct_subject, handle_originate_async, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to %s", direct_subject);
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: %s (direct async)", direct_subject);
    
    if (driver->subscribe(driver, "freeswitch.cmd.async.hangup", handle_hangup_async, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.cmd.async.hangup");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.cmd.async.hangup (broadcast async)");
    
    snprintf(direct_subject, sizeof(direct_subject), "freeswitch.cmd.async.hangup.%s", globals.node_id);
    if (driver->subscribe(driver, direct_subject, handle_hangup_async, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to %s", direct_subject);
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: %s (direct async)", direct_subject);
    
    return SWITCH_STATUS_SUCCESS;
}
