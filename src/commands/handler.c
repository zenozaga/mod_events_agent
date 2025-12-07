#include "../mod_event_agent.h"
#include "core.h"
#include "call.h"
#include "api.h"
#include "status.h"

static event_driver_t *g_driver = NULL;
static switch_mutex_t *g_mutex = NULL;

switch_status_t command_handler_init(event_driver_t *driver, switch_memory_pool_t *pool) {
    EVENT_LOG_INFO("Initializing command handler");
    
    g_driver = driver;
    
    if (switch_mutex_init(&g_mutex, SWITCH_MUTEX_NESTED, pool) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to create mutex");
        return SWITCH_STATUS_FALSE;
    }
    
    if (command_api_register(driver) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to register API commands");
        return SWITCH_STATUS_FALSE;
    }
    
    if (command_call_register(driver) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to register call commands");
        return SWITCH_STATUS_FALSE;
    }
    
    if (command_status_register(driver) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to register status commands");
        return SWITCH_STATUS_FALSE;
    }
    
    EVENT_LOG_INFO("Command handler initialized with 6 endpoints (1 generic + 3 sync + 2 async)");
    
    return SWITCH_STATUS_SUCCESS;
}

void command_handler_shutdown(void) {
    EVENT_LOG_INFO("Shutting down command handler");
    
    if (g_driver) {
        g_driver->unsubscribe(g_driver, "freeswitch.api");
        
        g_driver->unsubscribe(g_driver, "freeswitch.cmd.originate");
        g_driver->unsubscribe(g_driver, "freeswitch.cmd.hangup");
        g_driver->unsubscribe(g_driver, "freeswitch.cmd.status");
        
        g_driver->unsubscribe(g_driver, "freeswitch.cmd.async.originate");
        g_driver->unsubscribe(g_driver, "freeswitch.cmd.async.hangup");
    }
    
    g_driver = NULL;
    EVENT_LOG_INFO("Command handler shutdown complete");
}

void command_handler_get_stats(uint64_t *requests, uint64_t *success, uint64_t *failed) {
    command_stats_get(requests, success, failed);
}
