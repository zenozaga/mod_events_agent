#include "status.h"
#include "core.h"

static command_result_t handle_status_command(const command_request_t *request) {

    cJSON *log_level = request->payload ? cJSON_GetObjectItemCaseSensitive(request->payload, "log_level") : NULL;
    if (log_level && cJSON_IsString(log_level) && !switch_strlen_zero(log_level->valuestring)) {
        return command_result_error("Module-specific log levels were removed; use FreeSWITCH logging controls instead");
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

    command_result_t result = command_result_ok();
    result.message = "Module status";
    result.data = data_obj;
    return result;
}

switch_status_t command_status_register(void) {
    return command_register_handler("agent.status", handle_status_command);
}
