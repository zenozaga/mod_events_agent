#include "api.h"
#include "core.h"
#include <string.h>

static command_result_t handle_api_generic(const command_request_t *request) {
    const cJSON *args_item = cJSON_GetObjectItem(request->payload, "args");
    const char *args = (args_item && cJSON_IsString(args_item)) ? args_item->valuestring : NULL;

    switch_log_printf(SWITCH_CHANNEL_LOG,
                      SWITCH_LOG_DEBUG,
                      "[mod_event_agent] Generic API → %s %s",
                      request->command,
                      args ? args : "(no args)");

    switch_stream_handle_t stream = {0};
    SWITCH_STANDARD_STREAM(stream);

    switch_status_t status = switch_api_execute(request->command, args, NULL, &stream);
    const switch_bool_t has_error = (stream.data && strstr((char *)stream.data, "-ERR"));

    command_result_t result;
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        result = command_result_from_string(stream.data ? (char *)stream.data : "");
        result.message = "API command executed";
    } else {
        const char *error_msg = stream.data ? (char *)stream.data : "Unknown error";
        result = command_result_error(error_msg);
    }

    switch_safe_free(stream.data);
    return result;
}

switch_status_t command_api_register(void) {
    command_register_default_handler(handle_api_generic);
    return SWITCH_STATUS_SUCCESS;
}
