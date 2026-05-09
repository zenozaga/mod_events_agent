/*
 * test_validation.c — exercises the input validators on the typed
 * command handlers.
 *
 * Each case sends a payload that violates a specific validation rule
 * and asserts the module replies with the corresponding error
 * message. We do not assert on the exact wording of every error
 * (those are operator-facing and may change); we only require that
 * success is false and that the response is structured.
 *
 * The cases mirror the validators in src/commands/{call,dialplan}.c
 * plus the JSON parse step in src/commands/handler.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nats/nats.h>

static const char *get_env(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return (v && *v) ? v : fallback;
}

static int issue_raw(natsConnection *conn, const char *subject,
                     const char *payload, int len, char **reply_out)
{
    natsMsg *reply = NULL;
    natsStatus s = natsConnection_Request(&reply, conn, subject, payload, len, 3000);
    if (s != NATS_OK) {
        fprintf(stderr, "  request error: %s\n", natsStatus_GetText(s));
        return -1;
    }
    int rlen = natsMsg_GetDataLength(reply);
    char *buf = malloc(rlen + 1);
    memcpy(buf, natsMsg_GetData(reply), rlen);
    buf[rlen] = '\0';
    *reply_out = buf;
    natsMsg_Destroy(reply);
    return 0;
}

struct case_def {
    const char *name;
    const char *payload;
    /* What we expect: a substring that must appear in the response,
     * indicating the right validator caught the error. NULL means we
     * only require success=false. */
    const char *expect_substring;
};

static const struct case_def CASES[] = {
    /* Top-level JSON parse failures */
    { "invalid_json_garbage",        "not json at all", "Invalid JSON payload" },
    { "invalid_json_unterminated",   "{\"command\":\"agent.status\"",
                                                       "Invalid JSON payload" },
    { "missing_command_field",       "{\"args\":\"foo\"}",
                                                       "Missing 'command'" },
    { "command_not_string",          "{\"command\":42}",
                                                       "Missing 'command'" },

    /* originate validators (src/commands/call.c) */
    { "originate_missing_endpoint",  "{\"command\":\"originate\"}",
                                                       "endpoint must be" },
    { "originate_empty_endpoint",    "{\"command\":\"originate\",\"endpoint\":\"\"}",
                                                       "endpoint must be" },
    { "originate_missing_extension", "{\"command\":\"originate\",\"endpoint\":\"user/1000\"}",
                                                       "extension must be" },

    /* hangup validators */
    { "hangup_missing_uuid",         "{\"command\":\"hangup\"}",
                                                       "uuid must be" },

    /* dialplan.audio enum validation */
    { "dialplan_audio_bad_mode",     "{\"command\":\"dialplan.audio\",\"mode\":\"karaoke\"}",
                                                       "Invalid mode" },
    { "dialplan_audio_missing_mode", "{\"command\":\"dialplan.audio\"}",
                                                       "Invalid mode" },

    /* dialplan.autoanswer bool validation */
    { "dialplan_autoanswer_string",  "{\"command\":\"dialplan.autoanswer\",\"enabled\":\"yes\"}",
                                                       "enabled must be" },

    { NULL, NULL, NULL },
};

int main(int argc, char **argv) {
    const char *url = get_env("MOD_EVENT_AGENT_URL", "nats://localhost:4222");
    const char *node = get_env("MOD_EVENT_AGENT_NODE_ID", "fs-audit");
    char subject[256];
    snprintf(subject, sizeof(subject), "freeswitch.node.%s", node);

    natsConnection *conn = NULL;
    natsStatus s = natsConnection_ConnectTo(&conn, url);
    if (s != NATS_OK) {
        fprintf(stderr, "FAIL: cannot connect to %s: %s\n", url, natsStatus_GetText(s));
        return 1;
    }
    fprintf(stderr, "[test_validation] connected to %s, target=%s\n", url, subject);

    int failures = 0;
    for (int i = 0; CASES[i].name; i++) {
        char *reply = NULL;
        if (issue_raw(conn, subject, CASES[i].payload, strlen(CASES[i].payload), &reply) != 0) {
            fprintf(stderr, "FAIL: %s — request errored\n", CASES[i].name);
            failures++;
            continue;
        }
        if (strstr(reply, "\"success\":false") == NULL) {
            fprintf(stderr, "FAIL: %s — accepted invalid input: %s\n",
                    CASES[i].name, reply);
            failures++;
        } else if (CASES[i].expect_substring &&
                   strstr(reply, CASES[i].expect_substring) == NULL) {
            fprintf(stderr, "FAIL: %s — wrong reason. expected substring '%s', got: %s\n",
                    CASES[i].name, CASES[i].expect_substring, reply);
            failures++;
        } else {
            fprintf(stderr, "[test_validation] PASS %s\n", CASES[i].name);
        }
        free(reply);
    }

    natsConnection_Destroy(conn);
    fprintf(stderr, "[test_validation] %s\n", failures == 0 ? "ALL PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
