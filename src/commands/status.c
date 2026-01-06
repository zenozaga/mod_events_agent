#include "status.h"
#include "core.h"
#include "validation/validation.h"

typedef struct {
    char log_level[16];
} status_payload_t;

static const char *validate_status_payload(cJSON *json, status_payload_t *payload) {
    return v_enum_opt(json, payload, log_level,
                      "Invalid log_level value",
                      "console", "CONSOLE",
                      "alert",   "ALERT",
                      "crit",    "CRIT",
                      "error",   "ERROR",
                      "err",     "ERR",
                      "warning", "WARNING",
                      "notice",  "NOTICE",
                      "info",    "INFO",
                      "debug",   "DEBUG",
                      "emerg",   "EMERG");
}

static command_result_t handle_status_command(const command_request_t *request) {
    extern mod_event_agent_globals_t globals;

    status_payload_t payload = {0};
    const char *validation_error = validate_status_payload(request->payload, &payload);
    if (validation_error) {
        return command_result_error(validation_error);
    }

    switch_bool_t log_level_updated = SWITCH_FALSE;
    const char *message = "Module status";

    if (!switch_strlen_zero(payload.log_level)) {
        switch_log_level_t lvl = switch_log_str2level(payload.log_level);
        if (lvl == SWITCH_LOG_INVALID) {
            return command_result_error("Invalid log level value");
        }

        globals.log_level = lvl;
        log_level_updated = SWITCH_TRUE;
        message = "Log level updated";
        EVENT_LOG_INFO("Runtime log level set to %s",
                       switch_log_level2str(globals.log_level));
    }

    uint64_t requests_received = 0;
    uint64_t requests_success = 0;
    uint64_t requests_failed = 0;
    command_stats_get(&requests_received, &requests_success, &requests_failed);

    cJSON *data_obj = cJSON_CreateObject();
    if (!data_obj) {
        return command_result_error("Failed to allocate status payload");
    }

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

    command_result_t result = command_result_ok();
    result.message = message;
    result.data = data_obj;
    return result;
}

switch_status_t command_status_register(void) {
    return command_register_handler("agent.status", handle_status_command);
}
