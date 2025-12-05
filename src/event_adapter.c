/*
 * fs_event_adapter.c
 * FreeSWITCH event binding and handling
 */

#include "mod_event_agent.h"

static switch_bool_t should_publish_event(switch_event_t *event)
{
    const char *event_name;
    uint32_t i;

    if (!event) {
        return SWITCH_FALSE;
    }

    event_name = switch_event_name(event->event_id);

    /* Check exclude list first */
    if (globals.exclude_count > 0) {
        for (i = 0; i < globals.exclude_count; i++) {
            if (globals.exclude_events[i] && 
                !strcasecmp(event_name, globals.exclude_events[i])) {
                return SWITCH_FALSE;
            }
        }
    }

    /* If include list is specified, only publish those events */
    if (globals.include_count > 0) {
        for (i = 0; i < globals.include_count; i++) {
            if (globals.include_events[i] && 
                !strcasecmp(event_name, globals.include_events[i])) {
                return SWITCH_TRUE;
            }
        }
        return SWITCH_FALSE;
    }

    /* If publish_all_events is enabled and no include list, publish everything */
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
    
    /* Convert event name to lowercase and replace underscores with dots */
    switch_copy_string(lowercase_name, event_name, sizeof(lowercase_name));
    for (p = lowercase_name; *p; p++) {
        *p = tolower(*p);
        if (*p == '_') {
            *p = '.';
        }
    }

    /* Build subject: prefix.events.channel.create */
    subject = switch_mprintf("%s.events.%s", globals.subject_prefix, lowercase_name);

    return subject;
}

void event_callback(switch_event_t *event)
{
    char *json_str = NULL;
    char *subject = NULL;
    switch_status_t status;
    int num_subscribers = 0;

    if (!globals.running || !globals.driver || !globals.driver->is_connected(globals.driver)) {
        return;
    }

    if (!should_publish_event(event)) {
        return;
    }

    subject = build_subject(event);
    if (!subject) {
        EVENT_LOG_ERROR("Failed to build subject for event");
        return;
    }

    if (globals.driver->has_subscribers(globals.driver, subject, &num_subscribers) != SWITCH_STATUS_SUCCESS) {
        num_subscribers = 1;
    }
    
    if (num_subscribers == 0) {
        globals.events_skipped_no_subscribers++;
        switch_safe_free(subject);
        return;
    }

    json_str = serialize_event_to_json(event, globals.node_id);
    if (!json_str) {
        EVENT_LOG_ERROR("Failed to serialize event to JSON");
        switch_safe_free(subject);
        return;
    }

    status = globals.driver->publish(globals.driver, subject, json_str, strlen(json_str));
    if (status != SWITCH_STATUS_SUCCESS) {
        globals.events_failed++;
    } else {
        globals.events_published++;
        globals.bytes_published += strlen(json_str);
    }

    free_serialized_event(json_str);
    switch_safe_free(subject);
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
