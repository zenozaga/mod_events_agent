/*
 * command_handler.c  
 * Command subscription handler using driver interface
 */

#include "mod_event_agent.h"
#include <cjson/cJSON.h>

static event_driver_t *g_driver = NULL;
static switch_mutex_t *g_mutex = NULL;

typedef struct {
    uint64_t requests_received;
    uint64_t requests_success;
    uint64_t requests_failed;
} command_stats_t;

static command_stats_t g_stats = {0};

// Check if this node should process the request based on node_id filter
static switch_bool_t should_process_request(cJSON *json) {
    extern mod_event_agent_globals_t globals;
    
    // Get node_id from request (optional filter)
    cJSON *node_id_item = cJSON_GetObjectItem(json, "node_id");
    
    // If no node_id specified, process (broadcast mode)
    if (!node_id_item || !cJSON_IsString(node_id_item)) {
        return SWITCH_TRUE;
    }
    
    const char *target_node = node_id_item->valuestring;
    
    // If target_node matches our node_id, process it
    if (globals.node_id && strcmp(target_node, globals.node_id) == 0) {
        return SWITCH_TRUE;
    }
    
    // Target is for a different node, skip
    EVENT_LOG_DEBUG("Skipping request - target node: %s, our node: %s", 
                    target_node, globals.node_id ? globals.node_id : "unknown");
    return SWITCH_FALSE;
}

static char* build_json_response(switch_bool_t success, const char *message, const char *data) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "success", success);
    cJSON_AddStringToObject(json, "message", message ? message : "");
    if (data) cJSON_AddStringToObject(json, "data", data);
    cJSON_AddNumberToObject(json, "timestamp", (double)switch_time_now());
    
    // Add node_id to all responses for multi-node filtering
    extern mod_event_agent_globals_t globals;
    if (globals.node_id && strlen(globals.node_id) > 0) {
        cJSON_AddStringToObject(json, "node_id", globals.node_id);
    } else {
        cJSON_AddStringToObject(json, "node_id", "unknown");
    }
    
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return json_str;
}

static void handle_originate(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    char *response_str = NULL;
    
    g_stats.requests_received++;
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        response_str = build_json_response(SWITCH_FALSE, "Invalid JSON", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
        free(response_str);
        g_stats.requests_failed++;
        return;
    }
    
    // Check if this node should process the request
    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return; // Silently ignore - not for this node
    }
    
    cJSON *endpoint_json = cJSON_GetObjectItem(json, "endpoint");
    cJSON *extension_json = cJSON_GetObjectItem(json, "extension");
    
    if (!endpoint_json || !extension_json) {
        response_str = build_json_response(SWITCH_FALSE, "Missing required fields", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
        free(response_str);
        cJSON_Delete(json);
        g_stats.requests_failed++;
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
    
    // Check if FreeSWITCH returned an error (stream.data starts with "-ERR")
    switch_bool_t has_error = SWITCH_FALSE;
    if (stream.data && strncmp((char*)stream.data, "-ERR", 4) == 0) {
        has_error = SWITCH_TRUE;
    }
    
    if (status == SWITCH_STATUS_SUCCESS && stream.data && !has_error) {
        response_str = build_json_response(SWITCH_TRUE, "Call originated successfully", (char*)stream.data);
        g_stats.requests_success++;
    } else {
        const char *error_msg = stream.data ? (char*)stream.data : "Unknown error";
        response_str = build_json_response(SWITCH_FALSE, error_msg, NULL);
        g_stats.requests_failed++;
    }
    
    if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
    
    free(response_str);
    switch_safe_free(stream.data);
    cJSON_Delete(json);
}

static void handle_hangup(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    char *response_str = NULL;
    
    g_stats.requests_received++;
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        response_str = build_json_response(SWITCH_FALSE, "Invalid JSON", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
        free(response_str);
        g_stats.requests_failed++;
        return;
    }
    
    // Check if this node should process the request
    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return; // Silently ignore - not for this node
    }
    
    cJSON *uuid_json = cJSON_GetObjectItem(json, "uuid");
    if (!uuid_json) {
        response_str = build_json_response(SWITCH_FALSE, "Missing uuid", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
        free(response_str);
        cJSON_Delete(json);
        g_stats.requests_failed++;
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
    
    // Check if FreeSWITCH returned an error (stream.data starts with "-ERR")
    switch_bool_t has_error = SWITCH_FALSE;
    if (stream.data && strncmp((char*)stream.data, "-ERR", 4) == 0) {
        has_error = SWITCH_TRUE;
    }
    
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        response_str = build_json_response(SWITCH_TRUE, "Channel hangup successful", uuid);
        g_stats.requests_success++;
    } else {
        const char *error_msg = stream.data ? (char*)stream.data : "Unknown error";
        response_str = build_json_response(SWITCH_FALSE, error_msg, uuid);
        g_stats.requests_failed++;
    }
    
    if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
    
    free(response_str);
    switch_safe_free(stream.data);
    cJSON_Delete(json);
}

// Generic API handler - executes ANY FreeSWITCH API command
static void handle_api_generic(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    g_stats.requests_received++;
    
    // Parse request: {"command": "show", "args": "channels", "node_id": "fs-node-01"}
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        EVENT_LOG_ERROR("Invalid JSON in generic API request");
        char *error_response = build_json_response(SWITCH_FALSE, "Invalid JSON payload", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, error_response, strlen(error_response));
        free(error_response);
        g_stats.requests_failed++;
        return;
    }
    
    // Check if this node should process the request
    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return; // Silently ignore - not for this node
    }
    
    cJSON *cmd_item = cJSON_GetObjectItem(json, "command");
    cJSON *args_item = cJSON_GetObjectItem(json, "args");
    
    if (!cmd_item || !cJSON_IsString(cmd_item)) {
        EVENT_LOG_ERROR("Missing or invalid 'command' field");
        char *error_response = build_json_response(SWITCH_FALSE, "Missing or invalid 'command' field", NULL);
        if (reply_to) g_driver->publish(g_driver, reply_to, error_response, strlen(error_response));
        free(error_response);
        cJSON_Delete(json);
        g_stats.requests_failed++;
        return;
    }
    
    const char *command = cmd_item->valuestring;
    const char *args = (args_item && cJSON_IsString(args_item)) ? args_item->valuestring : NULL;
    
    EVENT_LOG_INFO("📡 Executing API command: %s %s", command, args ? args : "(no args)");
    
    // Execute API command
    switch_stream_handle_t stream = { 0 };
    SWITCH_STANDARD_STREAM(stream);
    switch_status_t status = switch_api_execute(command, args, NULL, &stream);
    
    // Check for errors in response data
    switch_bool_t has_error = SWITCH_FALSE;
    if (stream.data && strstr((char*)stream.data, "-ERR")) {
        has_error = SWITCH_TRUE;
    }
    
    // Build response
    char *response_str = NULL;
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        response_str = build_json_response(SWITCH_TRUE, "API command executed", stream.data ? (char*)stream.data : "");
        g_stats.requests_success++;
    } else {
        const char *error_msg = stream.data ? (char*)stream.data : "Unknown error";
        response_str = build_json_response(SWITCH_FALSE, error_msg, NULL);
        g_stats.requests_failed++;
    }
    
    // Send response
    if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
    
    // Cleanup
    free(response_str);
    switch_safe_free(stream.data);
    cJSON_Delete(json);
}

static void handle_status(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    g_stats.requests_received++;
    
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "success", SWITCH_TRUE);
    cJSON_AddStringToObject(json, "message", "System status");
    
    cJSON *stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "requests_received", (double)g_stats.requests_received);
    cJSON_AddNumberToObject(stats, "requests_success", (double)g_stats.requests_success);
    cJSON_AddNumberToObject(stats, "requests_failed", (double)g_stats.requests_failed);
    cJSON_AddItemToObject(json, "stats", stats);
    
    char *response_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    if (reply_to) g_driver->publish(g_driver, reply_to, response_str, strlen(response_str));
    
    free(response_str);
    g_stats.requests_success++;
}

// ASYNC handlers (fire-and-forget - NO respond even if reply_to is present)
static void handle_originate_async(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    g_stats.requests_received++;
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        EVENT_LOG_ERROR("Invalid JSON in async originate");
        g_stats.requests_failed++;
        return;
    }
    
    // Check if this node should process the request
    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return; // Silently ignore - not for this node
    }
    
    cJSON *endpoint_json = cJSON_GetObjectItem(json, "endpoint");
    cJSON *extension_json = cJSON_GetObjectItem(json, "extension");
    
    if (!endpoint_json || !extension_json) {
        EVENT_LOG_ERROR("Missing required fields in async originate");
        cJSON_Delete(json);
        g_stats.requests_failed++;
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
    
    // Check if FreeSWITCH returned an error
    switch_bool_t has_error = SWITCH_FALSE;
    if (stream.data && strncmp((char*)stream.data, "-ERR", 4) == 0) {
        has_error = SWITCH_TRUE;
    }
    
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        g_stats.requests_success++;
    } else {
        g_stats.requests_failed++;
        if (stream.data) {
            EVENT_LOG_ERROR((char*)stream.data);
        }
    }
    
    // NO send response (fire-and-forget)
    switch_safe_free(stream.data);
    cJSON_Delete(json);
}

static void handle_hangup_async(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data) {
    g_stats.requests_received++;
    
    cJSON *json = cJSON_Parse(data);
    if (!json) {
        EVENT_LOG_ERROR("Invalid JSON in async hangup");
        g_stats.requests_failed++;
        return;
    }
    
    // Check if this node should process the request
    if (!should_process_request(json)) {
        cJSON_Delete(json);
        return; // Silently ignore - not for this node
    }
    
    cJSON *uuid_json = cJSON_GetObjectItem(json, "uuid");
    if (!uuid_json) {
        EVENT_LOG_ERROR("Missing uuid in async hangup");
        cJSON_Delete(json);
        g_stats.requests_failed++;
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
    
    // Check if FreeSWITCH returned an error
    switch_bool_t has_error = SWITCH_FALSE;
    if (stream.data && strncmp((char*)stream.data, "-ERR", 4) == 0) {
        has_error = SWITCH_TRUE;
    }
    
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        g_stats.requests_success++;
    } else {
        g_stats.requests_failed++;
        if (stream.data) {
            EVENT_LOG_ERROR((char*)stream.data);
        }
    }
    
    // NO send response (fire-and-forget)
    switch_safe_free(stream.data);
    cJSON_Delete(json);
}

switch_status_t command_handler_init(event_driver_t *driver, switch_memory_pool_t *pool) {
    FILE *f = fopen("/var/log/command_handler_init.txt", "w");
    if (f) {
        fprintf(f, "command_handler_init CALLED\n");
        fclose(f);
    }
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "🚀 command_handler_init() CALLED\n");
    EVENT_LOG_INFO("Initializing command handler");
    
    g_driver = driver;
    
    if (switch_mutex_init(&g_mutex, SWITCH_MUTEX_NESTED, pool) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to create mutex");
        return SWITCH_STATUS_FALSE;
    }
    
    // Generic API handler (executes ANY FreeSWITCH command)
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "🔌 Subscribing to: freeswitch.api\n");
    if (driver->subscribe(driver, "freeswitch.api", handle_api_generic, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.api");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.api (generic API handler)");
    
    // Request-reply commands (sync)
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "🔌 Subscribing to: freeswitch.cmd.originate\n");
    if (driver->subscribe(driver, "freeswitch.cmd.originate", handle_originate, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.cmd.originate");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.cmd.originate");
    
    if (driver->subscribe(driver, "freeswitch.cmd.hangup", handle_hangup, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.cmd.hangup");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.cmd.hangup");
    
    if (driver->subscribe(driver, "freeswitch.cmd.status", handle_status, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.cmd.status");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.cmd.status");
    
    // Async commands (fire-and-forget - NO responses)
    if (driver->subscribe(driver, "freeswitch.cmd.async.originate", handle_originate_async, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.cmd.async.originate");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.cmd.async.originate (fire-and-forget)");
    
    if (driver->subscribe(driver, "freeswitch.cmd.async.hangup", handle_hangup_async, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to subscribe to freeswitch.cmd.async.hangup");
        return SWITCH_STATUS_FALSE;
    }
    EVENT_LOG_INFO("✓ Registered: freeswitch.cmd.async.hangup (fire-and-forget)");
    
    EVENT_LOG_INFO("Command handler initialized with 6 endpoints (1 generic + 3 sync + 2 async)");
    
    return SWITCH_STATUS_SUCCESS;
}

void command_handler_shutdown(void) {
    EVENT_LOG_INFO("Shutting down command handler");
    
    if (g_driver) {
        // Generic API handler
        g_driver->unsubscribe(g_driver, "freeswitch.api");
        
        // Sync commands
        g_driver->unsubscribe(g_driver, "freeswitch.cmd.originate");
        g_driver->unsubscribe(g_driver, "freeswitch.cmd.hangup");
        g_driver->unsubscribe(g_driver, "freeswitch.cmd.status");
        
        // Async commands
        g_driver->unsubscribe(g_driver, "freeswitch.cmd.async.originate");
        g_driver->unsubscribe(g_driver, "freeswitch.cmd.async.hangup");
    }
    
    g_driver = NULL;
    EVENT_LOG_INFO("Command handler shutdown complete");
}

void command_handler_get_stats(uint64_t *requests, uint64_t *success, uint64_t *failed) {
    if (requests) *requests = g_stats.requests_received;
    if (success) *success = g_stats.requests_success;
    if (failed) *failed = g_stats.requests_failed;
}
