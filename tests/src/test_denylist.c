/*
 * test_denylist.c — verifies the COMMAND_API_DENYLIST in commands/api.c.
 *
 * Each denied verb must come back with success=false and the
 * "Command is not permitted via the event bus" message. A non-denied
 * native verb (we use "uptime") must come back with success=true so
 * we can prove we are talking to a healthy module, not a broken one
 * that rejects everything.
 *
 * Keep this list in sync with COMMAND_API_DENYLIST. If a verb is added
 * there, add it here so the matrix stays exhaustive.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nats/nats.h>

static const char *get_env(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return (v && *v) ? v : fallback;
}

/* Verbs the module must reject. Mirrors src/commands/api.c
 * COMMAND_API_DENYLIST. */
static const char *DENIED_VERBS[] = {
    "shutdown",
    "fsctl",
    "load",
    "unload",
    "reload",
    "reloadxml",
    "reloadacl",
    "bgapi",
    "system",
    "bg_system",
    "lua",
    "luarun",
    "msleep",
    NULL,
};

/* Verbs that should be allowed through to the FS API. We pick small,
 * idempotent ones so the test is safe even on production-like hosts. */
static const char *ALLOWED_VERBS[] = {
    "uptime",
    "version",
    "status",
    NULL,
};

static int issue(natsConnection *conn, const char *subject,
                 const char *command, char **reply_out)
{
    char payload[256];
    int n = snprintf(payload, sizeof(payload),
                     "{\"command\":\"%s\"}", command);
    natsMsg *reply = NULL;
    natsStatus s = natsConnection_Request(&reply, conn, subject, payload, n, 3000);
    if (s != NATS_OK) {
        fprintf(stderr, "  request error for %s: %s\n",
                command, natsStatus_GetText(s));
        return -1;
    }
    int len = natsMsg_GetDataLength(reply);
    char *buf = malloc(len + 1);
    memcpy(buf, natsMsg_GetData(reply), len);
    buf[len] = '\0';
    *reply_out = buf;
    natsMsg_Destroy(reply);
    return 0;
}

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
    fprintf(stderr, "[test_denylist] connected to %s, target=%s\n", url, subject);

    int failures = 0;

    /* Each denied verb must come back rejected with the canonical
     * message. We assert on the message (not just success=false) so
     * that a test failure for some other reason — JSON parse error,
     * server-side error — does not mask a missing entry in the
     * denylist. */
    for (int i = 0; DENIED_VERBS[i]; i++) {
        char *reply = NULL;
        if (issue(conn, subject, DENIED_VERBS[i], &reply) != 0) {
            failures++;
            continue;
        }
        if (strstr(reply, "\"success\":false") == NULL) {
            fprintf(stderr, "FAIL: %s was NOT denied (response: %s)\n",
                    DENIED_VERBS[i], reply);
            failures++;
        } else if (strstr(reply, "Command is not permitted") == NULL) {
            fprintf(stderr, "FAIL: %s denied for the wrong reason: %s\n",
                    DENIED_VERBS[i], reply);
            failures++;
        } else {
            fprintf(stderr, "[test_denylist] PASS '%s' denied\n", DENIED_VERBS[i]);
        }
        free(reply);
    }

    /* Sanity floor: known-allowed verbs must still go through. If
     * these fail, the module is broken in a way unrelated to the
     * denylist, and the denied results above mean nothing. */
    for (int i = 0; ALLOWED_VERBS[i]; i++) {
        char *reply = NULL;
        if (issue(conn, subject, ALLOWED_VERBS[i], &reply) != 0) {
            failures++;
            continue;
        }
        if (strstr(reply, "\"success\":true") == NULL) {
            fprintf(stderr, "FAIL: %s should be allowed: %s\n",
                    ALLOWED_VERBS[i], reply);
            failures++;
        } else {
            fprintf(stderr, "[test_denylist] PASS '%s' allowed\n", ALLOWED_VERBS[i]);
        }
        free(reply);
    }

    natsConnection_Destroy(conn);
    fprintf(stderr, "[test_denylist] %s\n", failures == 0 ? "ALL PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
