#include "interface.h"
#include <nats/nats.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Hard ceiling on simultaneous NATS subscriptions held by the driver.
 * The module legitimately needs only two: the broadcast `<prefix>.api`
 * lane and the per-node `<prefix>.node.<id>` lane, plus a handful for
 * a future feature surface. A misbehaving caller (or a bug in the
 * resubscribe-after-reconnect path) could otherwise spin up
 * subscriptions until libnats runs out of file descriptors or the
 * NATS server kicks the connection. The cap surfaces the bug early
 * with a loud refusal instead of a silent leak. */
#define NATS_DRIVER_MAX_SUBSCRIPTIONS 32

/* Cap on the number of broker URLs accepted in a comma-separated
 * MOD_EVENT_AGENT_URL. Realistic NATS clusters run 3-5 nodes; 16 is
 * deliberate headroom to absorb a multi-region deployment without
 * letting a typo'd config grow unbounded. */
#define NATS_DRIVER_MAX_SERVERS 16

typedef struct {
    natsConnection *conn;
    natsOptions *opts;
    switch_hash_t *subscriptions;
    switch_mutex_t *mutex;
    switch_bool_t connected;
    uint64_t sent;
    uint64_t failed;
    uint64_t bytes;
    uint64_t reconnects;
} nats_driver_ctx_t;

typedef struct {
    message_handler_t handler;
    void *user_data;
    natsSubscription *sub;
} nats_subscription_t;

static void nats_connection_closed_cb(natsConnection *nc, void *closure) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)closure;
    ctx->connected = SWITCH_FALSE;
}

static void nats_disconnected_cb(natsConnection *nc, void *closure) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)closure;
    ctx->connected = SWITCH_FALSE;
}

static void nats_reconnected_cb(natsConnection *nc, void *closure) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)closure;
    ctx->connected = SWITCH_TRUE;
    ctx->reconnects++;
}

static void nats_error_cb(natsConnection *nc, natsSubscription *sub, natsStatus err, void *closure) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NATS async error: %s\n", natsStatus_GetText(err));
}

static void nats_message_cb(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure) {
    nats_subscription_t *nsub = (nats_subscription_t *)closure;
    const char *subject = natsMsg_GetSubject(msg);
    const char *data = natsMsg_GetData(msg);
    const char *reply = natsMsg_GetReply(msg);
    
    if (nsub && nsub->handler) {
        nsub->handler(subject, data, natsMsg_GetDataLength(msg), reply, nsub->user_data);
    }
    
    natsMsg_Destroy(msg);
}

static switch_status_t nats_init(event_driver_t *driver, switch_hash_t *config) {
    natsStatus s;
    nats_driver_ctx_t *ctx;
    const char *url, *token, *nkey_seed;

    ctx = switch_core_alloc(driver->pool, sizeof(nats_driver_ctx_t));
    memset(ctx, 0, sizeof(nats_driver_ctx_t));
    driver->handle = ctx;

    switch_core_hash_init(&ctx->subscriptions);
    switch_mutex_init(&ctx->mutex, SWITCH_MUTEX_NESTED, driver->pool);

    url = switch_core_hash_find(config, "url");
    if (!url) url = "nats://127.0.0.1:4222";

    s = natsOptions_Create(&ctx->opts);
    if (s != NATS_OK) {
        /* The hash was just initialised; tear it down so we do not leak
         * half-built state if the caller retries init or unloads. */
        switch_core_hash_destroy(&ctx->subscriptions);
        return SWITCH_STATUS_FALSE;
    }

    /* The url string can be either a single URL or a comma-separated
     * list of URLs that point at every node of a NATS cluster. libnats
     * has two different setters for this:
     *   - natsOptions_SetURL    : one URL, single-broker semantics
     *   - natsOptions_SetServers: array of URLs, the client tries each
     *     in order on connect and on reconnect, giving us cluster
     *     failover for free.
     *
     * We pick the right one based on whether the configured value
     * contains a comma. A list with a single entry could route
     * through SetServers too, but the dedicated SetURL path is
     * simpler and matches the common single-broker case. */
    if (strchr(url, ',')) {
        char *url_copy = strdup(url);
        if (!url_copy) {
            natsOptions_Destroy(ctx->opts);
            ctx->opts = NULL;
            switch_core_hash_destroy(&ctx->subscriptions);
            return SWITCH_STATUS_FALSE;
        }
        const char *servers[NATS_DRIVER_MAX_SERVERS];
        char *owned[NATS_DRIVER_MAX_SERVERS];
        int count = 0;
        char *save = NULL;
        char *tok = strtok_r(url_copy, ",", &save);
        while (tok && count < NATS_DRIVER_MAX_SERVERS) {
            /* Trim leading whitespace so "url1, url2" parses to two
             * clean entries instead of "url1" + " url2". libnats
             * rejects URLs with embedded spaces, so silent breakage
             * here would be confusing. */
            while (*tok && isspace((unsigned char)*tok)) tok++;
            /* Trim trailing whitespace too. */
            char *end = tok + strlen(tok);
            while (end > tok && isspace((unsigned char)*(end - 1))) {
                end--;
            }
            *end = '\0';
            if (*tok) {
                owned[count] = strdup(tok);
                servers[count] = owned[count];
                count++;
            }
            tok = strtok_r(NULL, ",", &save);
        }
        free(url_copy);

        if (count == 0) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                              "[mod_event_agent] No valid servers parsed from URL list");
            natsOptions_Destroy(ctx->opts);
            ctx->opts = NULL;
            switch_core_hash_destroy(&ctx->subscriptions);
            return SWITCH_STATUS_FALSE;
        }

        s = natsOptions_SetServers(ctx->opts, servers, count);

        /* libnats copies each URL string into its own internal
         * storage; we can release our temporary copies as soon as
         * SetServers returns regardless of whether it succeeded. */
        for (int i = 0; i < count; i++) {
            free(owned[i]);
        }

        if (s != NATS_OK) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                              "[mod_event_agent] natsOptions_SetServers failed: %s",
                              natsStatus_GetText(s));
            natsOptions_Destroy(ctx->opts);
            ctx->opts = NULL;
            switch_core_hash_destroy(&ctx->subscriptions);
            return SWITCH_STATUS_FALSE;
        }
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                          "[mod_event_agent] NATS configured with %d servers (cluster failover)",
                          count);
    } else {
        s = natsOptions_SetURL(ctx->opts, url);
        if (s != NATS_OK) {
            natsOptions_Destroy(ctx->opts);
            ctx->opts = NULL;
            switch_core_hash_destroy(&ctx->subscriptions);
            return SWITCH_STATUS_FALSE;
        }
    }
    
    token = switch_core_hash_find(config, "token");
    if (token && strlen(token) > 0) {
        natsOptions_SetToken(ctx->opts, token);
    }
    
    nkey_seed = switch_core_hash_find(config, "nkey_seed");
    if (nkey_seed && strlen(nkey_seed) > 0) {
        natsOptions_SetNKeyFromSeed(ctx->opts, NULL, nkey_seed);
    }
    
    natsOptions_SetClosedCB(ctx->opts, nats_connection_closed_cb, ctx);
    natsOptions_SetDisconnectedCB(ctx->opts, nats_disconnected_cb, ctx);
    natsOptions_SetReconnectedCB(ctx->opts, nats_reconnected_cb, ctx);
    natsOptions_SetErrorHandler(ctx->opts, nats_error_cb, ctx);
    natsOptions_SetMaxReconnect(ctx->opts, 60);
    natsOptions_SetReconnectWait(ctx->opts, 1000);
    natsOptions_SetReconnectBufSize(ctx->opts, 8 * 1024 * 1024);
    
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t nats_connect(event_driver_t *driver) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)driver->handle;
    natsStatus s;
    
    s = natsConnection_Connect(&ctx->conn, ctx->opts);
    if (s != NATS_OK) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NATS connect failed: %s\n", natsStatus_GetText(s));
        return SWITCH_STATUS_FALSE;
    }
    
    ctx->connected = SWITCH_TRUE;
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t nats_disconnect(event_driver_t *driver) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)driver->handle;
    
    if (ctx->conn) {
        natsConnection_Close(ctx->conn);
        ctx->connected = SWITCH_FALSE;
    }
    
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t nats_shutdown(event_driver_t *driver) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)driver->handle;
    
    if (ctx->conn) {
        natsConnection_Destroy(ctx->conn);
        ctx->conn = NULL;
    }
    
    if (ctx->opts) {
        natsOptions_Destroy(ctx->opts);
        ctx->opts = NULL;
    }
    
    if (ctx->subscriptions) {
        switch_core_hash_destroy(&ctx->subscriptions);
    }
    
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t nats_publish(event_driver_t *driver, const char *subject, const char *data, size_t len) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)driver->handle;
    natsStatus s;
    
    if (!ctx->conn || !ctx->connected) {
        ctx->failed++;
        return SWITCH_STATUS_FALSE;
    }
    
    s = natsConnection_Publish(ctx->conn, subject, (const void *)data, (int)len);
    if (s != NATS_OK) {
        ctx->failed++;
        return SWITCH_STATUS_FALSE;
    }
    
    ctx->sent++;
    ctx->bytes += len;
    return SWITCH_STATUS_SUCCESS;
}

/* Core NATS does not expose subscriber count without a server
 * round-trip; this driver always reports 1 so the caller will publish.
 * Kept on the driver interface in case a future driver (in-process
 * registry, JetStream stream info, etc.) implements it honestly. The
 * event hot-path was previously gating publishes on this returning 0,
 * which never happened — that branch has been removed so the cost of
 * this no-op call is gone. */
static switch_status_t nats_has_subscribers(event_driver_t *driver, const char *subject, int *count) {
    (void)driver;
    (void)subject;
    if (count) {
        *count = 1;
    }
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t nats_subscribe(event_driver_t *driver, const char *subject, message_handler_t handler, void *user_data) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)driver->handle;
    nats_subscription_t *nsub;
    natsStatus s;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] NATS subscribing to: %s", subject);

    /* Defensive double-subscribe guard. If the caller subscribes twice
     * to the same subject (e.g. on a reconnect-and-resubscribe flow
     * that did not unsubscribe first) we would leak the previous
     * natsSubscription handle and the previous nsub stays alive
     * holding a stale callback closure. Reject the duplicate so the
     * caller has to clean up explicitly.
     *
     * The same critical section also enforces NATS_DRIVER_MAX_SUBSCRIPTIONS:
     * we count the live subscriptions before adding a new one, and
     * refuse if we are at the cap. This stops a runaway subscribe
     * loop from exhausting fds / server memory before the operator
     * notices. */
    switch_mutex_lock(ctx->mutex);
    if (switch_core_hash_find(ctx->subscriptions, subject) != NULL) {
        switch_mutex_unlock(ctx->mutex);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                          "[mod_event_agent] NATS already subscribed to %s; refusing to overwrite", subject);
        return SWITCH_STATUS_FALSE;
    }
    /* Walk the hash to count entries. The hash is small (≤ cap) so
     * an O(n) walk on subscribe is fine and saves us a separate
     * counter that could drift on partial failure paths. */
    {
        switch_hash_index_t *hi;
        size_t live = 0;
        for (hi = switch_core_hash_first(ctx->subscriptions); hi; hi = switch_core_hash_next(&hi)) {
            live++;
            if (live >= NATS_DRIVER_MAX_SUBSCRIPTIONS) {
                switch_mutex_unlock(ctx->mutex);
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                                  "[mod_event_agent] NATS subscription cap reached (%d); refusing %s",
                                  NATS_DRIVER_MAX_SUBSCRIPTIONS, subject);
                /* hi may not be NULL when we break; there is no
                 * documented "free index" call, but the iterator is
                 * pool-backed and reclaimed on driver teardown, so
                 * this is safe. */
                return SWITCH_STATUS_FALSE;
            }
        }
    }
    switch_mutex_unlock(ctx->mutex);

    nsub = switch_core_alloc(driver->pool, sizeof(nats_subscription_t));
    nsub->handler = handler;
    nsub->user_data = user_data;

    s = natsConnection_Subscribe(&nsub->sub, ctx->conn, subject, nats_message_cb, nsub);
    if (s != NATS_OK) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] NATS failed to subscribe to %s: %s",
                          subject, natsStatus_GetText(s));
        /* nsub itself comes from the FreeSWITCH module pool; the pool
         * reclaims it on module shutdown. We do not have a per-alloc
         * free for switch_core_alloc, so we cannot release nsub here.
         * The trade-off is acceptable because subscribes only happen
         * at startup / on reconnect, both bounded events. */
        return SWITCH_STATUS_FALSE;
    }

    switch_mutex_lock(ctx->mutex);
    switch_core_hash_insert(ctx->subscriptions, subject, nsub);
    switch_mutex_unlock(ctx->mutex);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] NATS subscribed to: %s", subject);

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t nats_unsubscribe(event_driver_t *driver, const char *subject) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)driver->handle;
    nats_subscription_t *nsub;

    switch_mutex_lock(ctx->mutex);
    nsub = switch_core_hash_find(ctx->subscriptions, subject);
    if (nsub) {
        switch_core_hash_delete(ctx->subscriptions, subject);
    }
    switch_mutex_unlock(ctx->mutex);

    /* Destroy the subscription OUTSIDE the mutex. natsSubscription_Destroy
     * blocks until the message-callback thread drains; holding our own
     * mutex while it drains would deadlock if the callback ever tries to
     * acquire the same mutex (none do today, but the invariant is fragile
     * enough that we prefer not to depend on it). */
    if (nsub && nsub->sub) {
        natsSubscription_Destroy(nsub->sub);
        nsub->sub = NULL;
    }

    return SWITCH_STATUS_SUCCESS;
}

static switch_bool_t nats_is_connected(event_driver_t *driver) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)driver->handle;
    return ctx->connected;
}

static void nats_get_stats(event_driver_t *driver, uint64_t *sent, uint64_t *failed, uint64_t *bytes) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)driver->handle;
    if (sent) *sent = ctx->sent;
    if (failed) *failed = ctx->failed;
    if (bytes) *bytes = ctx->bytes;
}

event_driver_t *driver_nats_create(switch_memory_pool_t *pool) {
    event_driver_t *driver = switch_core_alloc(pool, sizeof(event_driver_t));
    
    driver->name = "nats";
    driver->pool = pool;
    driver->handle = NULL;
    driver->init = nats_init;
    driver->connect = nats_connect;
    driver->disconnect = nats_disconnect;
    driver->shutdown = nats_shutdown;
    driver->publish = nats_publish;
    driver->has_subscribers = nats_has_subscribers;
    driver->subscribe = nats_subscribe;
    driver->unsubscribe = nats_unsubscribe;
    driver->is_connected = nats_is_connected;
    driver->get_stats = nats_get_stats;
    
    return driver;
}
