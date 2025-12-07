#ifndef MOD_EVENT_AGENT_H
#define MOD_EVENT_AGENT_H

#include <switch.h>
#include "drivers/interface.h"
#include "core/logger.h"

#define MOD_EVENT_AGENT_VERSION "2.0.0"
#define DEFAULT_SUBJECT_PREFIX "freeswitch"

static inline void slugify_node_id(char *node_id) {
    if (!node_id) return;
    
    for (char *p = node_id; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = *p + ('a' - 'A');
        }
        else if (*p == '-' || *p == '.' || *p == '/' || *p == ' ') {
            *p = '_';
        }
        else if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) {
            *p = '_';
        }
    }
}

/* Forward declaration for dialplan manager */
typedef struct dialplan_manager_s dialplan_manager_t;

typedef struct {
    event_driver_t *driver;
    switch_memory_pool_t *pool;
    switch_mutex_t *mutex;
    switch_hash_t *config;
    
    char *driver_name;
    char *subject_prefix;
    char *node_id;
    switch_bool_t publish_all_events;
    
    char **include_events;
    char **exclude_events;
    uint32_t include_count;
    uint32_t exclude_count;
    
    switch_bool_t running;
    uint64_t events_published;
    uint64_t events_failed;
    uint64_t events_skipped_no_subscribers;
    time_t startup_time;
    uint64_t bytes_published;
    
    /* Dialplan manager */
    dialplan_manager_t *dialplan_manager;
    
} mod_event_agent_globals_t;

extern mod_event_agent_globals_t globals;

switch_status_t event_agent_config_load(switch_memory_pool_t *pool);
void event_agent_config_destroy(void);

switch_status_t event_adapter_init(void);
switch_status_t event_adapter_shutdown(void);
void event_callback(switch_event_t *event);

char *serialize_event_to_json(switch_event_t *event, const char *node_id);
void free_serialized_event(char *json);

switch_status_t command_handler_init(event_driver_t *driver, switch_memory_pool_t *pool);
void command_handler_shutdown(void);
void command_handler_get_stats(uint64_t *requests, uint64_t *success, uint64_t *failed);

#endif /* MOD_EVENT_AGENT_H */
