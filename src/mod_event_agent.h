/*
 * mod_event_agent.h
 * FreeSWITCH Event Agent - Pluggable Event Streaming Module
 * 
 * Supports multiple messaging backends: NATS, Kafka, RabbitMQ, Redis
 */

#ifndef MOD_EVENT_AGENT_H
#define MOD_EVENT_AGENT_H

#include <switch.h>
#include "driver_interface.h"

#define MOD_EVENT_AGENT_VERSION "2.0.0"
#define DEFAULT_SUBJECT_PREFIX "freeswitch"

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

void event_agent_log(switch_log_level_t level, const char *fmt, ...);

#define EVENT_LOG_DEBUG(fmt, ...) event_agent_log(SWITCH_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define EVENT_LOG_INFO(fmt, ...)  event_agent_log(SWITCH_LOG_INFO, fmt, ##__VA_ARGS__)
#define EVENT_LOG_WARNING(fmt, ...) event_agent_log(SWITCH_LOG_WARNING, fmt, ##__VA_ARGS__)
#define EVENT_LOG_ERROR(fmt, ...) event_agent_log(SWITCH_LOG_ERROR, fmt, ##__VA_ARGS__)

#endif /* MOD_EVENT_AGENT_H */
