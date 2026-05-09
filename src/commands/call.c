#include "call.h"

/* This file used to register typed handlers for "originate" and "hangup".
 * Both have been removed: they intercepted FreeSWITCH-native verbs that
 * the Go agent (and any ESL-compatible client) needs to send with their
 * full argument surface — variables, caller_id, timeout, dialplan,
 * context, etc. The typed handlers only validated three fields
 * (endpoint, extension, context) and dropped everything else, breaking
 * real call control.
 *
 * The module is now a transparent ESL-over-NATS bridge for these verbs:
 * a request like {"command":"originate","args":"{vars}endpoint &echo()"}
 * falls through to the generic API handler in api.c, which forwards
 * verb + args to switch_api_execute() — identical to what `fs_cli -x`
 * or ESL would do. Authorization belongs at the NATS layer (token /
 * NKey / subject ACL), not here.
 *
 * The function is kept so handler.c::command_handler_init compiles
 * unchanged. When the build system is next regenerated this file can
 * be dropped; for now keeping it minimal avoids touching Makefile.am. */

switch_status_t command_call_register(void) {
    return SWITCH_STATUS_SUCCESS;
}
