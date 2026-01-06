#include "mod_event_agent.h"

#define EVENT_FILTER_CAP 128

switch_status_t event_agent_config_load(switch_memory_pool_t *pool)
{
    switch_xml_t cfg, xml, settings, param;
    const char *name, *value;

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
            switch_core_hash_insert(globals.config, "url", switch_core_strdup(pool, value));
        }
        else if (!strcasecmp(name, "token")) {
            if (!zstr(value)) switch_core_hash_insert(globals.config, "token", switch_core_strdup(pool, value));
        }
        else if (!strcasecmp(name, "nkey_seed") || !strcasecmp(name, "nkey")) {
            if (!zstr(value)) switch_core_hash_insert(globals.config, "nkey_seed", switch_core_strdup(pool, value));
        }
        else if (!strcasecmp(name, "subject_prefix")) {
            globals.subject_prefix = switch_core_strdup(pool, value);
        }
        else if (!strcasecmp(name, "node_id")) {
            globals.node_id = switch_core_strdup(pool, value);
            slugify_node_id(globals.node_id);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Node ID slugified to: %s", globals.node_id);
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
