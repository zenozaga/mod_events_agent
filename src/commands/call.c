#include "call.h"
#include "core.h"
#include "validation/validation.h"
#include <string.h>

// ============================
// call.originate
// ============================

// Payload + Schema

typedef struct {
    char endpoint[256];
    char extension[256];
    char context[128];
} call_originate_payload_t;

static const char *validate_originate_payload(cJSON *json, call_originate_payload_t *payload) {
    const char *err = v_string(json, payload, endpoint,
                               v_len(1, 255),
                               "endpoint must be between 1 and 255 characters");
    if (err) {
        return err;
    }

    err = v_string(json, payload, extension,
                   v_len(1, 255),
                   "extension must be between 1 and 255 characters");
    if (err) {
        return err;
    }

    err = v_string_opt(json, payload, context,
                       v_len_max(127),
                       "context must be 127 characters or fewer");
    return err;
}

static command_result_t handle_originate_command(const command_request_t *request) {
    call_originate_payload_t payload = {0};
    const char *validation_error = validate_originate_payload(request->payload, &payload);
    if (validation_error) {
        return command_result_error(validation_error);
    }

    const char *context = switch_strlen_zero(payload.context) ? "default" : payload.context;

    char cmd[512];
    switch_snprintf(cmd, sizeof(cmd), "%s %s %s", payload.endpoint, payload.extension, context);

    switch_stream_handle_t stream = {0};
    SWITCH_STANDARD_STREAM(stream);

    switch_status_t status = switch_api_execute("originate", cmd, NULL, &stream);
    const switch_bool_t has_error = (stream.data && strncmp(stream.data, "-ERR", 4) == 0);

    command_result_t result;
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        result = command_result_from_string(stream.data ? stream.data : "");
        result.message = "Call originated successfully";
    } else {
        const char *error_msg = stream.data ? stream.data : "Unknown error";
        result = command_result_error(error_msg);
    }

    switch_safe_free(stream.data);
    return result;
}

// ============================
// call.hangup
// ============================

// Payload + Schema

typedef struct {
    char uuid[64];
    char cause[64];
} call_hangup_payload_t;

static const char *validate_hangup_payload(cJSON *json, call_hangup_payload_t *payload) {
    const char *err = v_string(json, payload, uuid,
                               v_len(2, 63),
                               "uuid must be between 2 and 63 characters");
    if (err) {
        return err;
    }

    err = v_string_opt(json, payload, cause,
                       v_len_max(63),
                       "cause must be 63 characters or fewer");
    return err;
}

static command_result_t handle_hangup_command(const command_request_t *request) {
    call_hangup_payload_t payload = {0};
    const char *validation_error = validate_hangup_payload(request->payload, &payload);
    if (validation_error) {
        return command_result_error(validation_error);
    }

    const char *cause = switch_strlen_zero(payload.cause) ? "NORMAL_CLEARING" : payload.cause;

    char cmd[256];
    switch_snprintf(cmd, sizeof(cmd), "%s %s", payload.uuid, cause);

    switch_stream_handle_t stream = {0};
    SWITCH_STANDARD_STREAM(stream);

    switch_status_t status = switch_api_execute("uuid_kill", cmd, NULL, &stream);
    const switch_bool_t has_error = (stream.data && strncmp(stream.data, "-ERR", 4) == 0);

    command_result_t result;
    if (status == SWITCH_STATUS_SUCCESS && !has_error) {
        result = command_result_from_string(payload.uuid);
        result.message = "Channel hangup successful";
    } else {
        const char *error_msg = stream.data ? stream.data : "Unknown error";
        result = command_result_error(error_msg);
    }

    switch_safe_free(stream.data);
    return result;
}

switch_status_t command_call_register(void) {
    if (command_register_handler("originate", handle_originate_command) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }
    if (command_register_handler("hangup", handle_hangup_command) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }
    return SWITCH_STATUS_SUCCESS;
}
