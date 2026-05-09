#include "api.h"
#include "core.h"
#include <string.h>

/* Denylist of FreeSWITCH API commands that are too dangerous to expose
 * over the message bus. This is a defense-in-depth layer on top of
 * NATS auth: even if a token is compromised or the broker is reachable
 * from somewhere unexpected, these specific commands cannot be issued
 * through the generic API fallback.
 *
 * `shutdown`, `fsctl shutdown`, `unload`/`load`/`reload` (modules), and
 * `bgapi` are the obvious foot-guns: shutdown stops the engine, module
 * commands can swap implementations at runtime, and `bgapi` would let
 * a caller bypass this very allowlist by wrapping the inner command.
 *
 * The check is on the leading API verb only; sub-arguments are not
 * filtered (they go to the API itself which can reject them).
 *
 * Specific commands the module exposes — `originate`, `hangup`,
 * `agent.status`, `dialplan.*` — are routed through their dedicated
 * registered handlers and never reach this fallback, so denying their
 * names here is irrelevant to legitimate flows. */
static const char *const COMMAND_API_DENYLIST[] = {
    "shutdown",
    "fsctl",        /* fsctl shutdown / fsctl reset / fsctl crash */
    "load",
    "unload",
    "reload",
    "reloadxml",    /* allowed via explicit reloadxml command if needed */
    "reloadacl",
    "bgapi",        /* could nest a denied command */
    "system",       /* shells out to the host OS */
    "bg_system",
    "lua",          /* eval arbitrary Lua */
    "luarun",
    "msleep",       /* DoS knob */
    NULL
};

static int command_is_denied(const char *command) {
    if (zstr(command)) {
        return 1;
    }
    for (size_t i = 0; COMMAND_API_DENYLIST[i] != NULL; i++) {
        if (strcasecmp(command, COMMAND_API_DENYLIST[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static command_result_t handle_api_generic(const command_request_t *request) {
    /* Generic-API fallback. Anything that is NOT registered as a typed
     * handler ends up here. Apply the denylist before reaching
     * switch_api_execute so a malicious / misbehaving publisher cannot
     * shutdown the engine or hot-swap modules through the bus. */
    if (command_is_denied(request->command)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                          "[mod_event_agent] Rejecting denied API command: %s", request->command);
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
