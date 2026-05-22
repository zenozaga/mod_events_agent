#ifndef COMMAND_EXECUTE_APP_H
#define COMMAND_EXECUTE_APP_H

#include <switch.h>

/* Registers the "execute_app" command handler.
 *
 * execute_app invokes a FreeSWITCH dialplan APP on a live channel.
 * APPs (registered via SWITCH_ADD_APP in their parent module) are
 * distinct from API verbs (SWITCH_ADD_API): the generic api handler
 * cannot reach them because switch_api_execute only resolves API
 * verbs. execute_app fills that gap by locating the channel session
 * and calling switch_core_session_execute_application directly —
 * the same C entry point dialplan XML uses internally.
 *
 * Expected payload:
 *   {
 *     "command": "execute_app",
 *     "uuid":    "<channel-uuid>",         // required
 *     "app":     "play_and_get_digits",    // required (any registered SWITCH_ADD_APP)
 *     "args":    "1 4 1 5000 # ...",       // optional (single string, spaces preserved)
 *     "async":   true|false,               // optional, standard semantics
 *     "forward_to": "<subject>"            // optional, standard side-channel
 *   }
 *
 * On success the response carries `data` empty plus message
 * "execute_app dispatched". The APP's own result (e.g. DTMF digits,
 * CHANNEL_EXECUTE_COMPLETE) flows via the normal event bus — callers
 * subscribe to the channel.* events to observe it.
 */
switch_status_t command_execute_app_register(void);

#endif
