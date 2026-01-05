#include "mod_event_agent.h"

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
        EVENT_LOG_WARNING("Failed to open event_agent.conf.xml, using defaults");
        return SWITCH_STATUS_SUCCESS;
    }

    if (!(settings = switch_xml_child(cfg, "settings"))) {
        EVENT_LOG_WARNING("No settings section in configuration, using defaults");
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
            EVENT_LOG_INFO("Node ID slugified to: %s", globals.node_id);
        }
        else if (!strcasecmp(name, "publish_all_events")) {
            globals.publish_all_events = switch_true(value);
        }
        else if (!strcasecmp(name, "include")) {
            if (!zstr(value)) {
                globals.include_count = switch_separate_string((char *)value, ',', 
                                                               globals.include_events, 100);
            }
        }
        else if (!strcasecmp(name, "exclude")) {
            if (!zstr(value)) {
                globals.exclude_count = switch_separate_string((char *)value, ',',
                                                               globals.exclude_events, 100);
            }
        }
    }

done:
    switch_xml_free(xml);
    
    EVENT_LOG_INFO("Configuration loaded - driver: %s, node: %s", 
                  globals.driver_name, globals.node_id);
    
    return SWITCH_STATUS_SUCCESS;
}

void event_agent_config_destroy(void)
{
    if (globals.config) {
        switch_core_hash_destroy(&globals.config);
    }
}
