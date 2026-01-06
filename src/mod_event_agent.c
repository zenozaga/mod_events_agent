#include "mod_event_agent.h"
#include "dialplan/manager.h"
#include "dialplan/commands.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_event_agent_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_agent_shutdown);
SWITCH_MODULE_DEFINITION(mod_event_agent, mod_event_agent_load, mod_event_agent_shutdown, NULL);

mod_event_agent_globals_t globals = {0};

SWITCH_MODULE_LOAD_FUNCTION(mod_event_agent_load)
{
    switch_status_t status;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Entering mod_event_agent_load");

    memset(&globals, 0, sizeof(globals));
    
    globals.pool = pool;
    globals.startup_time = time(NULL);
    
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Loading version %s", MOD_EVENT_AGENT_VERSION);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Loading configuration");
    status = event_agent_config_load(globals.pool);

    
    if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to load configuration");
        return SWITCH_STATUS_FALSE;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG,
                      SWITCH_LOG_NOTICE,
                      "[mod_event_agent] version %s starting (node=%s)",
                      MOD_EVENT_AGENT_VERSION,
                      globals.node_id ? globals.node_id : "unknown");

    if (strcasecmp(globals.driver_name, "nats") == 0) {
#ifdef WITH_NATS
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Creating NATS driver instance");
        globals.driver = driver_nats_create(globals.pool);
#else
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] NATS driver not compiled (need WITH_NATS=1)");
        return SWITCH_STATUS_FALSE;
#endif
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Unknown driver: %s (only 'nats' is supported)", globals.driver_name);
        return SWITCH_STATUS_FALSE;
    }
    
    if (!globals.driver) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to create driver: %s", globals.driver_name);
        return SWITCH_STATUS_FALSE;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Initializing %s driver", globals.driver->name);
    status = globals.driver->init(globals.driver, globals.config);
    if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to initialize driver");
        return SWITCH_STATUS_FALSE;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Connecting %s driver", globals.driver->name);
    status = globals.driver->connect(globals.driver);
    if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to connect driver");
        globals.driver->shutdown(globals.driver);
        return SWITCH_STATUS_FALSE;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Initializing event adapter");
    status = event_adapter_init();
    if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to initialize event adapter");
        globals.driver->disconnect(globals.driver);
        globals.driver->shutdown(globals.driver);
        return SWITCH_STATUS_FALSE;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Initializing dialplan manager");
    status = dialplan_manager_init(&globals.dialplan_manager, globals.pool);
    if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] Failed to initialize dialplan manager (dialplan control disabled)");
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Dialplan manager initialized - park mode available");
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Initializing command handler");
    status = command_handler_init(globals.driver, globals.pool, globals.dialplan_manager);
    
    if (status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] Failed to initialize command handler (commands disabled)");
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_INFO,
                          "[mod_event_agent] Command handler enabled on %s.api and %s.node.%s",
                          globals.subject_prefix ? globals.subject_prefix : DEFAULT_SUBJECT_PREFIX,
                          globals.subject_prefix ? globals.subject_prefix : DEFAULT_SUBJECT_PREFIX,
                          globals.node_id ? globals.node_id : "<any>");
    }

    globals.running = SWITCH_TRUE;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Module loaded successfully (driver: %s)", globals.driver->name);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Publishing events to: %s.events.*", globals.subject_prefix);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] mod_event_agent_load completed successfully");

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_agent_shutdown)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Entering mod_event_agent_shutdown");
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Shutting down mod_event_agent");

    globals.running = SWITCH_FALSE;

    command_handler_shutdown();
    event_adapter_shutdown();

    if (globals.dialplan_manager) {
        dialplan_manager_shutdown(globals.dialplan_manager);
        globals.dialplan_manager = NULL;
    }

    if (globals.driver) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Disconnecting driver %s", globals.driver->name);
        globals.driver->disconnect(globals.driver);
        globals.driver->shutdown(globals.driver);
    }

    event_agent_config_destroy();

    switch_log_printf(SWITCH_CHANNEL_LOG,
                      SWITCH_LOG_INFO,
                      "[mod_event_agent] Shutdown complete. Published %llu events (failed: %llu, skipped: %llu)",
                      (unsigned long long)globals.events_published,
                      (unsigned long long)globals.events_failed,
                      (unsigned long long)globals.events_skipped_no_subscribers);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] mod_event_agent_shutdown completed");
    return SWITCH_STATUS_SUCCESS;
}
