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
    const char *event_name = NULL;

    if (!event) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] event_callback invoked with NULL event");
        return;
    }

    event_name = switch_event_name(event->event_id);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] event_callback entered (%s)", event_name ? event_name : "unknown");

    if (!globals.running || !globals.driver || !globals.driver->is_connected(globals.driver)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Skipping event %s: driver not ready", event_name ? event_name : "unknown");
        return;
    }

    if (!should_publish_event(event)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Event %s filtered out (include/exclude rules)", event_name ? event_name : "unknown");
        return;
    }

    subject = build_subject(event);
    if (!subject) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to build subject for event %s", event_name ? event_name : "unknown");
        return;
    }

    /* Subscriber-count check has been removed from the hot path. The
     * NATS driver could not (and core NATS cannot) cheaply tell us
     * whether anyone is listening without an extra round-trip, so the
     * old has_subscribers() always returned 1 and the skip-branch was
     * dead. We always publish; NATS itself drops messages with no
     * subscribers in O(1) on the broker side.
     *
     * The events_skipped_no_subscribers counter stays in the globals
     * struct for ABI stability (dialplan/status command reports it),
     * but it will only ever increment if a future driver implements
     * has_subscribers honestly. */

    json_str = serialize_event_to_json(event, globals.node_id);
    if (!json_str) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to serialize event %s to JSON", event_name ? event_name : "unknown");
        switch_safe_free(subject);
        return;
    }

    size_t payload_len = strlen(json_str);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Publishing event %s to %s (%zu bytes)", event_name ? event_name : "unknown", subject, payload_len);

    status = globals.driver->publish(globals.driver, subject, json_str, payload_len);
    if (status != SWITCH_STATUS_SUCCESS) {
        globals.events_failed++;
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] Driver failed to publish event %s to %s", event_name ? event_name : "unknown", subject);
    } else {
        globals.events_published++;
        globals.bytes_published += payload_len;
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] Event %s published successfully", event_name ? event_name : "unknown");
    }

    /* Best-effort tee: channels tagged with nats_forward_to get every
     * event mirrored to that subject, reusing the serialized bytes. */
    const char *fwd = switch_event_get_header(event, "variable_nats_forward_to");
    if (fwd && !switch_strlen_zero(fwd)) {
        if (globals.driver->publish(globals.driver, fwd, json_str, payload_len) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] Forward publish of event %s to %s failed", event_name ? event_name : "unknown", fwd);
        }
    }

    free_serialized_event(json_str);
    switch_safe_free(subject);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[mod_event_agent] event_callback exit (%s)", event_name ? event_name : "unknown");
}

switch_status_t event_adapter_init(void)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Initializing event adapter");

    if (switch_event_bind("mod_event_agent", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY,
                         event_callback, NULL) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[mod_event_agent] Failed to bind to FreeSWITCH events");
        return SWITCH_STATUS_FALSE;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Successfully bound to FreeSWITCH events");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t event_adapter_shutdown(void)
{
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Shutting down event adapter");
    
    switch_event_unbind_callback(event_callback);
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Event adapter shutdown complete");
    return SWITCH_STATUS_SUCCESS;
}
