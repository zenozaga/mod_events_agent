/*
 * test_size_limit.c — verifies the COMMAND_MAX_PAYLOAD_BYTES guard.
 *
 * This is the integration test the audit identified as "untestable
 * via nats CLI" because shells truncate args near ARG_MAX. Going
 * through libnats directly bypasses the shell entirely, so we can
 * issue a real >64 KB request and assert the module rejects it.
 *
 * Pass criteria:
 *   - Sending a payload <= 64 KB returns success.
 *   - Sending a payload >  64 KB returns {"success":false,
 *                                          "message":"Payload too large"}.
 *
 * Tunables read from the environment so the harness in run.sh can
 * point this at any broker / node.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nats/nats.h>

/* Mirrors the constant in src/commands/handler.c — keep in sync if
 * the module ever raises its cap. */
#define MODULE_MAX_PAYLOAD (64 * 1024)

static const char *get_env(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return (v && *v) ? v : fallback;
}

static int request_payload(natsConnection *conn, const char *subject,
                           const char *payload, int payload_len,
                           char **reply_out, int *reply_len_out)
{
    natsMsg *reply = NULL;
    natsStatus s = natsConnection_Request(&reply, conn, subject,
                                          payload, payload_len, 3000);
    if (s != NATS_OK) {
        fprintf(stderr, "request failed: %s\n", natsStatus_GetText(s));
        return -1;
    }
    int len = natsMsg_GetDataLength(reply);
    char *buf = malloc(len + 1);
    if (!buf) { natsMsg_Destroy(reply); return -1; }
    memcpy(buf, natsMsg_GetData(reply), len);
    buf[len] = '\0';
    *reply_out = buf;
    *reply_len_out = len;
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
        fprintf(stderr, "FAIL: cannot connect to %s: %s\n",
                url, natsStatus_GetText(s));
        return 1;
    }
    fprintf(stderr, "[test_size_limit] connected to %s, target=%s\n", url, subject);

    int failures = 0;

    /* Case 1: small payload should succeed. Floor sanity check so a
     * red result on this case rules out broker / module unreachability
     * before we blame the size guard. */
    {
        const char *small = "{\"command\":\"agent.status\"}";
        char *reply = NULL;
        int rlen = 0;
        if (request_payload(conn, subject, small, strlen(small), &reply, &rlen) != 0) {
            fprintf(stderr, "FAIL: small payload request errored\n");
            failures++;
        } else {
            if (strstr(reply, "\"success\":true") == NULL) {
                fprintf(stderr, "FAIL: small payload was not accepted: %s\n", reply);
                failures++;
            } else {
                fprintf(stderr, "[test_size_limit] PASS small payload accepted\n");
            }
            free(reply);
        }
    }

    /* Case 2: payload just under the cap should still succeed. We
     * pad the JSON with a benign string field so cJSON parses it as
     * valid input. */
    {
        int pad_size = MODULE_MAX_PAYLOAD - 64; /* leave headroom for the JSON envelope */
        char *payload = malloc(pad_size + 256);
        char *pad = malloc(pad_size + 1);
        if (!payload || !pad) {
            fprintf(stderr, "FAIL: oom building under-cap payload\n");
            failures++;
        } else {
            memset(pad, 'A', pad_size);
            pad[pad_size] = '\0';
            int n = snprintf(payload, pad_size + 256,
                             "{\"command\":\"agent.status\",\"junk\":\"%s\"}", pad);
            char *reply = NULL;
            int rlen = 0;
            if (request_payload(conn, subject, payload, n, &reply, &rlen) != 0) {
                fprintf(stderr, "FAIL: under-cap request errored\n");
                failures++;
            } else {
                if (strstr(reply, "\"success\":true") == NULL) {
                    fprintf(stderr, "FAIL: under-cap (%d bytes) rejected: %s\n", n, reply);
                    failures++;
                } else {
                    fprintf(stderr, "[test_size_limit] PASS under-cap (%d bytes) accepted\n", n);
                }
                free(reply);
            }
        }
        free(payload);
        free(pad);
    }

    /* Case 3: the actual hardening — payload above the cap must be
     * rejected with a "Payload too large" message and never reach
     * cJSON_Parse. We send 70 KB. */
    {
        int pad_size = (MODULE_MAX_PAYLOAD + 6 * 1024); /* clearly over */
        char *payload = malloc(pad_size + 256);
        char *pad = malloc(pad_size + 1);
        if (!payload || !pad) {
            fprintf(stderr, "FAIL: oom building over-cap payload\n");
            failures++;
        } else {
            memset(pad, 'A', pad_size);
            pad[pad_size] = '\0';
            int n = snprintf(payload, pad_size + 256,
                             "{\"command\":\"agent.status\",\"junk\":\"%s\"}", pad);
            char *reply = NULL;
            int rlen = 0;
            if (request_payload(conn, subject, payload, n, &reply, &rlen) != 0) {
                fprintf(stderr, "FAIL: over-cap request errored\n");
                failures++;
            } else {
                /* Both conditions matter: rejection AND the specific
                 * error message we emit. A different rejection text
                 * (e.g. "Invalid JSON") would mean the size guard
                 * never fired and cJSON tried to parse a 70 KB blob. */
                if (strstr(reply, "\"success\":false") == NULL) {
                    fprintf(stderr, "FAIL: over-cap (%d bytes) was accepted: %s\n", n, reply);
                    failures++;
                } else if (strstr(reply, "Payload too large") == NULL) {
                    fprintf(stderr, "FAIL: over-cap rejected for the wrong reason: %s\n", reply);
                    failures++;
                } else {
                    fprintf(stderr, "[test_size_limit] PASS over-cap (%d bytes) rejected\n", n);
                }
                free(reply);
            }
        }
        free(payload);
        free(pad);
    }

    natsConnection_Destroy(conn);
    fprintf(stderr, "[test_size_limit] %s\n", failures == 0 ? "ALL PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
