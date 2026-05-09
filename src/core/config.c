#include "mod_event_agent.h"
#include <stdlib.h>

#define EVENT_FILTER_CAP 128

/* Centralised env-var names. Secrets (token, nkey seed) live in env so a
 * leaked event_agent.conf.xml never carries credentials. The code prefers
 * env over XML when both are set; XML stays a fallback for non-secret
 * fields (url, prefix, node id, event filters). */
#define ENV_TOKEN     "MOD_EVENT_AGENT_TOKEN"
#define ENV_NKEY_SEED "MOD_EVENT_AGENT_NKEY_SEED"
#define ENV_URL       "MOD_EVENT_AGENT_URL"
#define ENV_NODE_ID   "MOD_EVENT_AGENT_NODE_ID"

/* Helper: read env var and copy into the module pool when non-empty. */
static const char *env_strdup(switch_memory_pool_t *pool, const char *name)
{
    const char *raw = getenv(name);
    if (zstr(raw)) {
        return NULL;
    }
    return switch_core_strdup(pool, raw);
}

switch_status_t event_agent_config_load(switch_memory_pool_t *pool)
{
    switch_xml_t cfg, xml, settings, param;
    const char *name, *value;
    const char *env_token = NULL;
    const char *env_nkey = NULL;
    const char *env_url = NULL;
    const char *env_node = NULL;

    switch_core_hash_init(&globals.config);

    globals.driver_name = "nats";
    globals.subject_prefix = switch_core_strdup(pool, DEFAULT_SUBJECT_PREFIX);
    globals.node_id = switch_core_sprintf(pool, "fs-node-%s", switch_core_get_switchname());
    slugify_node_id(globals.node_id);
    globals.publish_all_events = SWITCH_TRUE;
    globals.include_events = NULL;
    globals.exclude_events = NULL;
    globals.include_count = 0;
    globals.exclude_count = 0;

    switch_core_hash_insert(globals.config, "url", "nats://127.0.0.1:4222");

    /* Read env vars upfront. They take precedence over event_agent.conf.xml.
     * Secrets (token, nkey seed) SHOULD only come from env so a checked-in
     * config file never carries credentials. The XML branches preserve
     * backwards compatibility for development setups. */
    env_token = env_strdup(pool, ENV_TOKEN);
    env_nkey  = env_strdup(pool, ENV_NKEY_SEED);
    env_url   = env_strdup(pool, ENV_URL);
    env_node  = env_strdup(pool, ENV_NODE_ID);

    if (env_url) {
        switch_core_hash_insert(globals.config, "url", (char *)env_url);
    }
    if (env_node) {
        globals.node_id = (char *)env_node;
        slugify_node_id(globals.node_id);
    }
    if (env_token) {
        switch_core_hash_insert(globals.config, "token", (char *)env_token);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                          "[mod_event_agent] auth token sourced from env %s", ENV_TOKEN);
    }
    if (env_nkey) {
        switch_core_hash_insert(globals.config, "nkey_seed", (char *)env_nkey);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                          "[mod_event_agent] auth nkey sourced from env %s", ENV_NKEY_SEED);
    }

    if (!(xml = switch_xml_open_cfg("event_agent.conf", &cfg, NULL))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] Failed to open event_agent.conf.xml, using defaults");
        return SWITCH_STATUS_SUCCESS;
    }

    if (!(settings = switch_xml_child(cfg, "settings"))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] No settings section in configuration, using defaults");
        goto done;
    }

    for (param = switch_xml_child(settings, "param"); param; param = param->next) {
        name = switch_xml_attr_soft(param, "name");
        value = switch_xml_attr_soft(param, "value");

        if (zstr(name) || zstr(value)) continue;

        if (!strcasecmp(name, "driver")) {
            globals.driver_name = switch_core_strdup(pool, value);
        }
        else if (!strcasecmp(name, "url") || !strcasecmp(name, "host")) {
            /* Env var wins. Skip the XML override if MOD_EVENT_AGENT_URL was
             * set so the deployment surface stays single-sourced. */
            if (!env_url) {
                switch_core_hash_insert(globals.config, "url", switch_core_strdup(pool, value));
            }
        }
        else if (!strcasecmp(name, "token")) {
            /* Token must come from env. The XML branch is kept for legacy
             * configs but emits a loud warning so operators migrate. */
            if (env_token) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                                  "[mod_event_agent] ignoring XML token (env %s set)", ENV_TOKEN);
            } else if (!zstr(value)) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                                  "[mod_event_agent] DEPRECATED: token in XML config; "
                                  "move to env %s for production", ENV_TOKEN);
                switch_core_hash_insert(globals.config, "token", switch_core_strdup(pool, value));
            }
        }
        else if (!strcasecmp(name, "nkey_seed") || !strcasecmp(name, "nkey")) {
            if (env_nkey) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                                  "[mod_event_agent] ignoring XML nkey (env %s set)", ENV_NKEY_SEED);
            } else if (!zstr(value)) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
                                  "[mod_event_agent] DEPRECATED: nkey in XML config; "
                                  "move to env %s for production", ENV_NKEY_SEED);
                switch_core_hash_insert(globals.config, "nkey_seed", switch_core_strdup(pool, value));
            }
        }
        else if (!strcasecmp(name, "subject_prefix")) {
            globals.subject_prefix = switch_core_strdup(pool, value);
        }
        else if (!strcasecmp(name, "node_id")) {
            if (!env_node) {
                globals.node_id = switch_core_strdup(pool, value);
                slugify_node_id(globals.node_id);
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Node ID slugified to: %s", globals.node_id);
            }
        }
        else if (!strcasecmp(name, "publish_all_events")) {
            globals.publish_all_events = switch_true(value);
        }
        else if (!strcasecmp(name, "include")) {
            globals.include_count = 0;
            globals.include_events = NULL;
            if (!zstr(value)) {
                char *include_copy = switch_core_strdup(pool, value);
                char **include_slots = switch_core_alloc(pool, sizeof(char *) * EVENT_FILTER_CAP);
                memset(include_slots, 0, sizeof(char *) * EVENT_FILTER_CAP);
                globals.include_events = include_slots;
                globals.include_count = switch_separate_string(include_copy, ',', include_slots, EVENT_FILTER_CAP);
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Configured %u include event filters", globals.include_count);
            }
        }
        else if (!strcasecmp(name, "exclude")) {
            globals.exclude_count = 0;
            globals.exclude_events = NULL;
            if (!zstr(value)) {
                char *exclude_copy = switch_core_strdup(pool, value);
                char **exclude_slots = switch_core_alloc(pool, sizeof(char *) * EVENT_FILTER_CAP);
                memset(exclude_slots, 0, sizeof(char *) * EVENT_FILTER_CAP);
                globals.exclude_events = exclude_slots;
                globals.exclude_count = switch_separate_string(exclude_copy, ',', exclude_slots, EVENT_FILTER_CAP);
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Configured %u exclude event filters", globals.exclude_count);
            }
        }
    }

done:
    switch_xml_free(xml);
    
    switch_log_printf(SWITCH_CHANNEL_LOG,
                      SWITCH_LOG_INFO,
                      "[mod_event_agent] Configuration loaded - driver: %s, node: %s",
                      globals.driver_name,
                      globals.node_id);
    
    return SWITCH_STATUS_SUCCESS;
}

void event_agent_config_destroy(void)
{
    if (globals.config) {
        switch_core_hash_destroy(&globals.config);
    }
}
