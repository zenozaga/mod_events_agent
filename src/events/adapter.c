#include "mod_event_agent.h"

static switch_bool_t should_publish_event(switch_event_t *event)
{
    const char *event_name;
    uint32_t i;

    if (!event) {
        return SWITCH_FALSE;
    }

    event_name = switch_event_name(event->event_id);

    if (globals.exclude_count > 0) {
        for (i = 0; i < globals.exclude_count; i++) {
            if (globals.exclude_events[i] && 
                !strcasecmp(event_name, globals.exclude_events[i])) {
                return SWITCH_FALSE;
            }
        }
    }

    if (globals.include_count > 0) {
        for (i = 0; i < globals.include_count; i++) {
            if (globals.include_events[i] && 
                !strcasecmp(event_name, globals.include_events[i])) {
                return SWITCH_TRUE;
            }
        }
        return SWITCH_FALSE;
    }

    return globals.publish_all_events;
}

static char *build_subject(switch_event_t *event)
{
    const char *event_name;
    char *subject;
    char lowercase_name[256];
    char *p;

    if (!event) {
        return NULL;
    }

    event_name = switch_event_name(event->event_id);
    
    switch_copy_string(lowercase_name, event_name, sizeof(lowercase_name));
    for (p = lowercase_name; *p; p++) {
        *p = tolower(*p);
        if (*p == '_') {
            *p = '.';
        }
    }

    subject = switch_mprintf("%s.events.%s", globals.subject_prefix, lowercase_name);

    return subject;
}

void event_callback(switch_event_t *event)
{
    char *json_str = NULL;
    char *subject = NULL;
    switch_status_t status;
    int num_subscribers = 0;
    const char *event_name = NULL;

    if (!event) {
        EVENT_LOG_WARNING("event_callback invoked with NULL event");
        return;
    }

    event_name = switch_event_name(event->event_id);
    EVENT_LOG_DEBUG("event_callback entered (%s)", event_name ? event_name : "unknown");

    if (!globals.running || !globals.driver || !globals.driver->is_connected(globals.driver)) {
        EVENT_LOG_DEBUG("Skipping event %s: driver not ready", event_name ? event_name : "unknown");
        return;
    }

    if (!should_publish_event(event)) {
        EVENT_LOG_DEBUG("Event %s filtered out (include/exclude rules)", event_name ? event_name : "unknown");
        return;
    }

    subject = build_subject(event);
    if (!subject) {
        EVENT_LOG_ERROR("Failed to build subject for event %s", event_name ? event_name : "unknown");
        return;
    }

    if (globals.driver->has_subscribers(globals.driver, subject, &num_subscribers) != SWITCH_STATUS_SUCCESS) {
        num_subscribers = 1;
    }
    
    if (num_subscribers == 0) {
        globals.events_skipped_no_subscribers++;
        EVENT_LOG_DEBUG("Skipping event %s: no subscribers on %s", event_name ? event_name : "unknown", subject);
        switch_safe_free(subject);
        return;
    }

    json_str = serialize_event_to_json(event, globals.node_id);
    if (!json_str) {
        EVENT_LOG_ERROR("Failed to serialize event %s to JSON", event_name ? event_name : "unknown");
        switch_safe_free(subject);
        return;
    }

    size_t payload_len = strlen(json_str);
    EVENT_LOG_DEBUG("Publishing event %s to %s (%zu bytes)", event_name ? event_name : "unknown", subject, payload_len);

    status = globals.driver->publish(globals.driver, subject, json_str, payload_len);
    if (status != SWITCH_STATUS_SUCCESS) {
        globals.events_failed++;
        EVENT_LOG_WARNING("Driver failed to publish event %s to %s", event_name ? event_name : "unknown", subject);
    } else {
        globals.events_published++;
        globals.bytes_published += payload_len;
        EVENT_LOG_DEBUG("Event %s published successfully", event_name ? event_name : "unknown");
    }

    free_serialized_event(json_str);
    switch_safe_free(subject);
    EVENT_LOG_DEBUG("event_callback exit (%s)", event_name ? event_name : "unknown");
}

switch_status_t event_adapter_init(void)
{
    EVENT_LOG_INFO("Initializing event adapter");

    if (switch_event_bind("mod_event_agent", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY,
                         event_callback, NULL) != SWITCH_STATUS_SUCCESS) {
        EVENT_LOG_ERROR("Failed to bind to FreeSWITCH events");
        return SWITCH_STATUS_FALSE;
    }

    EVENT_LOG_INFO("Successfully bound to FreeSWITCH events");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t event_adapter_shutdown(void)
{
    EVENT_LOG_INFO("Shutting down event adapter");
    
    switch_event_unbind_callback(event_callback);
    
    EVENT_LOG_INFO("Event adapter shutdown complete");
    return SWITCH_STATUS_SUCCESS;
}
