#include "interface.h"
#include <nats/nats.h>

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
    if (s != NATS_OK) return SWITCH_STATUS_FALSE;
    
    s = natsOptions_SetURL(ctx->opts, url);
    if (s != NATS_OK) {
        natsOptions_Destroy(ctx->opts);
        return SWITCH_STATUS_FALSE;
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

static switch_status_t nats_has_subscribers(event_driver_t *driver, const char *subject, int *count) {
    *count = 1;
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t nats_subscribe(event_driver_t *driver, const char *subject, message_handler_t handler, void *user_data) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)driver->handle;
    nats_subscription_t *nsub;
    natsStatus s;
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "📨 [NATS] Subscribing to: %s\n", subject);
    
    nsub = switch_core_alloc(driver->pool, sizeof(nats_subscription_t));
    nsub->handler = handler;
    nsub->user_data = user_data;
    
    s = natsConnection_Subscribe(&nsub->sub, ctx->conn, subject, nats_message_cb, nsub);
    if (s != NATS_OK) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "❌ [NATS] Failed to subscribe to %s: %s\n", subject, natsStatus_GetText(s));
        return SWITCH_STATUS_FALSE;
    }
    
    switch_mutex_lock(ctx->mutex);
    switch_core_hash_insert(ctx->subscriptions, subject, nsub);
    switch_mutex_unlock(ctx->mutex);
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "✅ [NATS] Subscribed to: %s\n", subject);
    
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t nats_unsubscribe(event_driver_t *driver, const char *subject) {
    nats_driver_ctx_t *ctx = (nats_driver_ctx_t *)driver->handle;
    nats_subscription_t *nsub;
    
    switch_mutex_lock(ctx->mutex);
    nsub = switch_core_hash_find(ctx->subscriptions, subject);
    if (nsub) {
        natsSubscription_Destroy(nsub->sub);
        switch_core_hash_delete(ctx->subscriptions, subject);
    }
    switch_mutex_unlock(ctx->mutex);
    
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
