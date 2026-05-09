#include "api.h"
#include "core.h"
#include "../mod_event_agent.h"
#include <string.h>

/* Generic-API fallback for verbs the module does not register a typed
 * handler for. The intent of mod_event_agent is to be a transparent
 * ESL-over-NATS bridge — anything you can do with `fs_cli` or ESL you
 * can also do here. Authorization (who is allowed to publish what)
 * lives in the broker (NATS token / NKey / subject ACLs / JWT
 * claims), not in this file.
 *
 * The optional denylist below is a defense-in-depth knob for
 * deployments that want to pin specific verbs (sandbox public access,
 * compliance, dev environments). It defaults to EMPTY so out-of-the
 * -box behaviour is "forward everything"; operators opt in by
 * populating MOD_EVENT_AGENT_API_DENYLIST or the matching XML param.
 *
 * Specific commands the module exposes through typed handlers
 * (`originate`, `hangup`, `agent.status`, `dialplan.*`, etc.) bypass
 * this fallback entirely — putting their names in the denylist has
 * no effect on those flows.
 */

static int command_is_denied(const char *command) {
    if (zstr(command)) {
        /* Empty command is always rejected; the dispatcher should
         * never reach us with one, but we guard anyway. */
        return 1;
    }
    if (globals.api_denylist_count == 0 || globals.api_denylist == NULL) {
        return 0;
    }
    for (uint32_t i = 0; i < globals.api_denylist_count; i++) {
        const char *entry = globals.api_denylist[i];
        if (entry && strcasecmp(command, entry) == 0) {
            return 1;
        }
    }
    return 0;
}

static command_result_t handle_api_generic(const command_request_t *request) {
    if (command_is_denied(request->command)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                          "[mod_event_agent] Rejecting denylisted API command: %s",
                          request->command);
        return command_result_error("Command is not permitted via the event bus");
    }

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
