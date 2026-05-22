#include "execute_app.h"
#include "core.h"
#include "../mod_event_agent.h"

/* execute_app — invoke a FreeSWITCH dialplan APP on a live channel.
 *
 * Why this exists: mod_event_agent's generic api handler delegates to
 * switch_api_execute(), which only resolves verbs registered via
 * SWITCH_ADD_API. Apps (SWITCH_ADD_APP) — including play_and_get_digits,
 * bridge, ivr, detect_speech, record, transfer, etc — are invisible to
 * that path because they expect a session context, not a stream. The
 * historical workaround (uuid_broadcast <uuid> app::args aleg) truncates
 * multi-arg apps on the first space inside the args, making it useless
 * for play_and_get_digits and similar multi-arg apps.
 *
 * This handler closes that gap by performing the same call dialplan
 * uses internally:
 *
 *     session = switch_core_session_locate(uuid);
 *     switch_core_session_execute_application(session, app, args);
 *     switch_core_session_rwunlock(session);
 *
 * Args are passed VERBATIM as a single string to the app, so spaces in
 * any positional argument are preserved exactly. The app's own parser
 * handles tokenization the way the app expects (typically
 * switch_separate_string with ' ' delimiter inside the app's
 * standard_app_function).
 *
 * Failure modes:
 *   - missing uuid / app → 400-style error in the response.
 *   - session not found (channel already gone, wrong uuid) → error
 *     "session not found", session pointer never locked.
 *   - app execution failure → propagated via the FS log; the response
 *     reports "switch_core_session_execute_application returned X".
 *     The app's actual outcome arrives later on the event bus as
 *     CHANNEL_EXECUTE_COMPLETE — not in this response.
 */
static command_result_t handle_execute_app(const command_request_t *request) {
    const cJSON *uuid_item = cJSON_GetObjectItemCaseSensitive(request->payload, "uuid");
    if (!uuid_item || !cJSON_IsString(uuid_item) || switch_strlen_zero(uuid_item->valuestring)) {
        return command_result_error("execute_app: 'uuid' field required");
    }
    const cJSON *app_item = cJSON_GetObjectItemCaseSensitive(request->payload, "app");
    if (!app_item || !cJSON_IsString(app_item) || switch_strlen_zero(app_item->valuestring)) {
        return command_result_error("execute_app: 'app' field required");
    }

    const char *uuid = uuid_item->valuestring;
    const char *app  = app_item->valuestring;

    /* args is optional — some apps take none (e.g. "answer", "park"). */
    const cJSON *args_item = cJSON_GetObjectItemCaseSensitive(request->payload, "args");
    const char *args = (args_item && cJSON_IsString(args_item)) ? args_item->valuestring : "";

    /* switch_core_session_locate increments the session refcount; we
     * MUST call rwunlock on every exit path to avoid leaking a hold
     * that prevents the session from being destroyed at hangup. */
    switch_core_session_t *session = switch_core_session_locate(uuid);
    if (!session) {
        return command_result_error("execute_app: session not found");
    }

    /* Use the async variant: switch_core_session_execute_application is
     * SYNCHRONOUS — it blocks the calling thread until the app returns.
     * For long-running apps (play_and_get_digits waits for DTMF;
     * playback waits for the file to finish) that would tie up the
     * mod_event_agent dispatch thread AND make the NATS request/reply
     * time out before the reply could be sent. The _async variant
     * queues the app on the session's own runtime thread and returns
     * immediately — same primitive uuid_broadcast uses internally.
     *
     * The app's actual outcome (digits captured, file played, etc.)
     * flows back via the normal event bus as CHANNEL_EXECUTE_COMPLETE;
     * callers subscribe to channel.execute_complete with the matching
     * Unique-ID to observe completion. */
    switch_status_t status = switch_core_session_execute_application_async(session, app, args);
    switch_core_session_rwunlock(session);

    if (status != SWITCH_STATUS_SUCCESS) {
        return command_result_error("execute_app: async dispatch returned non-success");
    }

    command_result_t result = command_result_ok();
    result.message = "execute_app dispatched";
    return result;
}

switch_status_t command_execute_app_register(void) {
    return command_register_handler("execute_app", handle_execute_app);
}
