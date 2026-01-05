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

    memset(&globals, 0, sizeof(globals));
    
    globals.pool = pool;
    globals.startup_time = time(NULL);
    
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

    EVENT_LOG_INFO("Loading mod_event_agent version %s", MOD_EVENT_AGENT_VERSION);

    status = event_agent_config_load(globals.pool);
    if (status != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to load configuration");
        return SWITCH_STATUS_FALSE;
    }

    if (strcasecmp(globals.driver_name, "nats") == 0) {
#ifdef WITH_NATS
        globals.driver = driver_nats_create(globals.pool);
#else
        EVENT_LOG_ERROR("NATS driver not compiled (need WITH_NATS=1)");
        return SWITCH_STATUS_FALSE;
#endif
    } else {
        EVENT_LOG_ERROR("Unknown driver: %s (only 'nats' is supported)", globals.driver_name);
        return SWITCH_STATUS_FALSE;
    }
    
    if (!globals.driver) {
        EVENT_LOG_ERROR("Failed to create driver: %s", globals.driver_name);
        return SWITCH_STATUS_FALSE;
    }

    status = globals.driver->init(globals.driver, globals.config);
    if (status != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to initialize driver");
        return SWITCH_STATUS_FALSE;
    }

    status = globals.driver->connect(globals.driver);
    if (status != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to connect driver");
        globals.driver->shutdown(globals.driver);
        return SWITCH_STATUS_FALSE;
    }

    status = event_adapter_init();
    if (status != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to initialize event adapter");
        globals.driver->disconnect(globals.driver);
        globals.driver->shutdown(globals.driver);
        return SWITCH_STATUS_FALSE;
    }

    status = command_handler_init(globals.driver, globals.pool);
    
    if (status != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_WARNING("Failed to initialize command handler (commands disabled)");
    } else {
        EVENT_LOG_INFO("Command handler enabled - listening on %s.cmd.*",
                globals.subject_prefix ? globals.subject_prefix : DEFAULT_SUBJECT_PREFIX);
    }
    
    status = dialplan_manager_init(&globals.dialplan_manager, globals.pool);
    if (status != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_WARNING("Failed to initialize dialplan manager (dialplan control disabled)");
    } else {
        EVENT_LOG_INFO("Dialplan manager initialized - park mode available");
        
        status = command_dialplan_init(globals.driver, globals.dialplan_manager);
        if (status != SWITCH_STATUS_SUCCESS) {
            EVENT_LOG_WARNING("Failed to initialize dialplan commands");
        } else {
            EVENT_LOG_INFO("Dialplan commands enabled - listening on %s.cmd.dialplan.*",
                            globals.subject_prefix ? globals.subject_prefix : DEFAULT_SUBJECT_PREFIX);
        }
    }

    globals.running = SWITCH_TRUE;

    EVENT_LOG_INFO("mod_event_agent loaded successfully (driver: %s)", globals.driver->name);
    EVENT_LOG_INFO("Publishing events to: %s.events.*", globals.subject_prefix);

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_agent_shutdown)
{
    EVENT_LOG_INFO("Shutting down mod_event_agent");

    globals.running = SWITCH_FALSE;

    if (globals.dialplan_manager) {
        command_dialplan_shutdown(globals.driver);
        dialplan_manager_shutdown(globals.dialplan_manager);
        globals.dialplan_manager = NULL;
    }

    command_handler_shutdown();
    event_adapter_shutdown();

    if (globals.driver) {
        globals.driver->disconnect(globals.driver);
        globals.driver->shutdown(globals.driver);
    }

    event_agent_config_destroy();

    EVENT_LOG_INFO("Shutdown complete. Published %llu events (failed: %llu, skipped: %llu)",
                 (unsigned long long)globals.events_published,
                 (unsigned long long)globals.events_failed,
                 (unsigned long long)globals.events_skipped_no_subscribers);

    return SWITCH_STATUS_SUCCESS;
}
