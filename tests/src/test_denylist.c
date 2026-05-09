/*
 * test_denylist.c — verifies the optional API denylist in commands/api.c.
 *
 * The denylist is configured via the MOD_EVENT_AGENT_API_DENYLIST env var
 * (or the matching XML param). The default is empty: out of the box the
 * module forwards every API verb to switch_api_execute, behaving as a
 * transparent ESL-over-NATS bridge. Operators opt into the denylist when
 * they want a guardrail at the module layer (sandbox public access,
 * compliance, dev environments).
 *
 * This test runs in two modes:
 *
 *   1. With MOD_EVENT_AGENT_API_DENYLIST set: every verb on the list
 *      must come back with success=false and the canonical
 *      "Command is not permitted via the event bus" message. Both the
 *      FS container AND this test must see the same env value, since
 *      the FS process is the one enforcing it.
 *
 *   2. Without the env var: the test exits 0 with a SKIP message. We
 *      do not assert "all verbs are forwarded" because verbs like
 *      `shutdown` or `fsctl shutdown` would actually stop the engine
 *      if they reached it — that scenario is for an isolated CI box
 *      with mod_loopback, not a generic test runner.
 *
 * Always-allowed verbs (uptime, version, status) are still smoke-checked
 * regardless of mode so we know we are talking to a healthy module
 * before drawing conclusions about the denial path.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <nats/nats.h>

#define MAX_DENYLIST 64

static const char *get_env(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return (v && *v) ? v : fallback;
}

/* Split a comma-separated env value into argv-style slots. The caller
 * passes a writable copy of the env value because strtok mutates it. */
static int split_csv(char *buf, char **out, int max) {
    int count = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok && count < max; tok = strtok_r(NULL, ",", &save)) {
        while (*tok && isspace((unsigned char)*tok)) tok++;
        char *end = tok + strlen(tok);
        while (end > tok && isspace((unsigned char)*(end - 1))) *(--end) = '\0';
        if (*tok) out[count++] = tok;
    }
    return count;
}

static const char *ALLOWED_VERBS[] = { "uptime", "version", "status", NULL };

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
    const char *denylist_env = getenv("MOD_EVENT_AGENT_API_DENYLIST");

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

    /* Sanity floor first: a healthy module must still serve simple
     * read-only verbs. If this floor fails, the rest of the test
     * cannot draw any conclusions. */
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

    if (failures > 0) {
        fprintf(stderr, "[test_denylist] sanity floor failed; aborting\n");
        natsConnection_Destroy(conn);
        return 1;
    }

    /* Opt-in branch: only assert denial when the env is actually set.
     * Otherwise the module is in transparent-bridge mode and there is
     * nothing to check at the module layer (authorization is the
     * broker's job). */
    if (!denylist_env || !*denylist_env) {
        fprintf(stderr,
                "[test_denylist] SKIP — MOD_EVENT_AGENT_API_DENYLIST not set; "
                "module is in transparent-bridge mode (default)\n");
        natsConnection_Destroy(conn);
        return 0;
    }

    char *denylist_copy = strdup(denylist_env);
    char *denied_verbs[MAX_DENYLIST];
    int denied_count = split_csv(denylist_copy, denied_verbs, MAX_DENYLIST);

    fprintf(stderr,
            "[test_denylist] denylist mode: %d entries from %s\n",
            denied_count, "MOD_EVENT_AGENT_API_DENYLIST");

    for (int i = 0; i < denied_count; i++) {
        char *reply = NULL;
        if (issue(conn, subject, denied_verbs[i], &reply) != 0) {
            failures++;
            continue;
        }
        if (strstr(reply, "\"success\":false") == NULL) {
            fprintf(stderr, "FAIL: %s was NOT denied (response: %s)\n",
                    denied_verbs[i], reply);
            failures++;
        } else if (strstr(reply, "Command is not permitted") == NULL) {
            fprintf(stderr, "FAIL: %s denied for the wrong reason: %s\n",
                    denied_verbs[i], reply);
            failures++;
        } else {
            fprintf(stderr, "[test_denylist] PASS '%s' denied\n", denied_verbs[i]);
        }
        free(reply);
    }

    free(denylist_copy);
    natsConnection_Destroy(conn);
    fprintf(stderr, "[test_denylist] %s\n", failures == 0 ? "ALL PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
