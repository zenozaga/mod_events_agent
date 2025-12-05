/*
 * driver_interface.h
 * Abstract driver interface for event streaming backends
 * 
 * Supports multiple messaging systems: NATS, Kafka, RabbitMQ, Redis, etc.
 */

#ifndef DRIVER_INTERFACE_H
#define DRIVER_INTERFACE_H

#include <switch.h>

typedef struct event_driver_s event_driver_t;
typedef void (*message_handler_t)(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data);

struct event_driver_s {
    const char *name;
    void *handle;
    switch_memory_pool_t *pool;
    
    switch_status_t (*init)(event_driver_t *driver, switch_hash_t *config);
    switch_status_t (*connect)(event_driver_t *driver);
    switch_status_t (*disconnect)(event_driver_t *driver);
    switch_status_t (*shutdown)(event_driver_t *driver);
    
    switch_status_t (*publish)(event_driver_t *driver, const char *subject, const char *data, size_t len);
    switch_status_t (*has_subscribers)(event_driver_t *driver, const char *subject, int *count);
    
    switch_status_t (*subscribe)(event_driver_t *driver, const char *subject, message_handler_t handler, void *user_data);
    switch_status_t (*unsubscribe)(event_driver_t *driver, const char *subject);
    
    switch_bool_t (*is_connected)(event_driver_t *driver);
    void (*get_stats)(event_driver_t *driver, uint64_t *sent, uint64_t *failed, uint64_t *bytes);
};

event_driver_t *driver_create(const char *name);
void driver_destroy(event_driver_t *driver);

#ifdef WITH_NATS
event_driver_t *driver_nats_create(switch_memory_pool_t *pool);
#endif

#ifdef WITH_NATS_RAW
event_driver_t *driver_nats_raw_create(switch_memory_pool_t *pool);
#endif

#ifdef WITH_KAFKA
event_driver_t *driver_kafka_create(void);
#endif

#ifdef WITH_RABBITMQ
event_driver_t *driver_rabbitmq_create(void);
#endif

#ifdef WITH_REDIS
event_driver_t *driver_redis_create(void);
#endif

#endif /* DRIVER_INTERFACE_H */
