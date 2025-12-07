#include "manager.h"
#include <switch.h>

/* XML search function - called by FreeSWITCH when looking for dialplan */
static switch_xml_t dialplan_xml_fetch(const char *section, 
                                        const char *tag_name, 
                                        const char *key_name, 
                                        const char *key_value,
                                        switch_event_t *params, 
                                        void *user_data)
{
    dialplan_manager_t *manager = (dialplan_manager_t *)user_data;
    switch_xml_t xml = NULL;
    switch_xml_t section_xml = NULL;
    switch_xml_t context_xml = NULL;
    switch_xml_t extension_xml = NULL;
    switch_xml_t condition_xml = NULL;
    switch_xml_t action_xml = NULL;
    
    if (!manager || manager->mode == DIALPLAN_MODE_DISABLED) {
        return NULL; /* Let normal dialplan handle it */
    }
    
    switch_mutex_lock(manager->mutex);
    
    /* Only intercept dialplan section */
    if (zstr(section) || strcasecmp(section, "dialplan")) {
        switch_mutex_unlock(manager->mutex);
        return NULL;
    }
    
    /* Create root XML structure */
    xml = switch_xml_new("document");
    switch_xml_set_attr_d(xml, "type", "freeswitch/xml");
    
    section_xml = switch_xml_add_child_d(xml, "section", 0);
    switch_xml_set_attr_d(section_xml, "name", "dialplan");
    
    context_xml = switch_xml_add_child_d(section_xml, "context", 0);
    switch_xml_set_attr_d(context_xml, "name", manager->context_name);
    
    /* Create park extension */
    extension_xml = switch_xml_add_child_d(context_xml, "extension", 0);
    switch_xml_set_attr_d(extension_xml, "name", "event_agent_park");
    
    condition_xml = switch_xml_add_child_d(extension_xml, "condition", 0);
    switch_xml_set_attr_d(condition_xml, "field", "destination_number");
    switch_xml_set_attr_d(condition_xml, "expression", "^(.+)$");
    
    /* Set variables */
    action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
    switch_xml_set_attr_d(action_xml, "application", "set");
    switch_xml_set_attr_d(action_xml, "data", "hangup_after_bridge=true");
    
    action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
    switch_xml_set_attr_d(action_xml, "application", "set");
    switch_xml_set_attr_d(action_xml, "data", "continue_on_fail=true");
    
    /* Auto answer if enabled */
    if (manager->auto_answer) {
        action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
        switch_xml_set_attr_d(action_xml, "application", "answer");
        switch_xml_set_attr_d(action_xml, "data", "");
    }
    
    /* Audio mode configuration */
    switch (manager->audio_mode) {
        case AUDIO_MODE_SILENCE:
            /* No audio, just silence */
            action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
            switch_xml_set_attr_d(action_xml, "application", "set");
            switch_xml_set_attr_d(action_xml, "data", "park_timeout=0");
            break;
            
        case AUDIO_MODE_RINGBACK:
            /* Play ringback tone */
            action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
            switch_xml_set_attr_d(action_xml, "application", "ring_ready");
            switch_xml_set_attr_d(action_xml, "data", "");
            break;
            
        case AUDIO_MODE_MUSIC:
            /* Play music on hold */
            if (!zstr(manager->music_class)) {
                char moh_data[256];
                switch_snprintf(moh_data, sizeof(moh_data), "playback:local_stream://%s", manager->music_class);
                
                action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
                switch_xml_set_attr_d(action_xml, "application", "set");
                switch_xml_set_attr_d(action_xml, "data", "hold_music=local_stream://moh");
                
                action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
                switch_xml_set_attr_d(action_xml, "application", "answer");
                switch_xml_set_attr_d(action_xml, "data", "");
                
                action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
                switch_xml_set_attr_d(action_xml, "application", "playback");
                switch_xml_set_attr_d(action_xml, "data", moh_data);
            } else {
                /* Default music */
                action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
                switch_xml_set_attr_d(action_xml, "application", "answer");
                switch_xml_set_attr_d(action_xml, "data", "");
                
                action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
                switch_xml_set_attr_d(action_xml, "application", "playback");
                switch_xml_set_attr_d(action_xml, "data", "$${hold_music}");
            }
            break;
    }
    
    /* Park the call - waits for external command */
    action_xml = switch_xml_add_child_d(condition_xml, "action", 0);
    switch_xml_set_attr_d(action_xml, "application", "park");
    switch_xml_set_attr_d(action_xml, "data", "");
    
    /* Update statistics */
    manager->calls_intercepted++;
    if (manager->mode == DIALPLAN_MODE_PARK) {
        manager->calls_parked++;
    }
    
    switch_mutex_unlock(manager->mutex);
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "Event Agent: Intercepted call, mode=%s, audio=%s, auto_answer=%s\n",
                     manager->mode == DIALPLAN_MODE_PARK ? "park" : "disabled",
                     manager->audio_mode == AUDIO_MODE_SILENCE ? "silence" :
                     manager->audio_mode == AUDIO_MODE_RINGBACK ? "ringback" : "music",
                     manager->auto_answer ? "yes" : "no");
    
    return xml;
}

switch_status_t dialplan_manager_init(dialplan_manager_t **manager, switch_memory_pool_t *pool)
{
    dialplan_manager_t *m = NULL;
    
    if (!manager || !pool) {
        return SWITCH_STATUS_FALSE;
    }
    
    m = switch_core_alloc(pool, sizeof(dialplan_manager_t));
    if (!m) {
        return SWITCH_STATUS_MEMERR;
    }
    
    memset(m, 0, sizeof(dialplan_manager_t));
    m->pool = pool;
    
    switch_mutex_init(&m->mutex, SWITCH_MUTEX_NESTED, pool);
    
    /* Default configuration */
    m->mode = DIALPLAN_MODE_DISABLED;
    m->audio_mode = AUDIO_MODE_RINGBACK;
    m->auto_answer = SWITCH_FALSE;
    m->context_name = switch_core_strdup(pool, "default");
    m->music_class = switch_core_strdup(pool, "moh");
    
    /* Bind to dialplan section */
    if (switch_xml_bind_search_function_ret(dialplan_xml_fetch, 
                                            SWITCH_XML_SECTION_DIALPLAN,
                                            m, 
                                            &m->binding) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                         "Failed to bind XML search function for dialplan\n");
        return SWITCH_STATUS_FALSE;
    }
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "Dialplan Manager initialized (mode=disabled)\n");
    
    *manager = m;
    return SWITCH_STATUS_SUCCESS;
}

void dialplan_manager_shutdown(dialplan_manager_t *manager)
{
    if (!manager) {
        return;
    }
    
    if (manager->binding) {
        switch_xml_unbind_search_function(&manager->binding);
        manager->binding = NULL;
    }
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "Dialplan Manager shutdown (intercepted=%u, parked=%u)\n",
                     manager->calls_intercepted, manager->calls_parked);
}

switch_status_t dialplan_manager_set_mode(dialplan_manager_t *manager, dialplan_mode_t mode)
{
    if (!manager) {
        return SWITCH_STATUS_FALSE;
    }
    
    switch_mutex_lock(manager->mutex);
    manager->mode = mode;
    switch_mutex_unlock(manager->mutex);
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "Dialplan Manager: mode changed to %s\n",
                     mode == DIALPLAN_MODE_PARK ? "PARK" : "DISABLED");
    
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t dialplan_manager_set_audio_mode(dialplan_manager_t *manager, audio_mode_t audio_mode)
{
    if (!manager) {
        return SWITCH_STATUS_FALSE;
    }
    
    switch_mutex_lock(manager->mutex);
    manager->audio_mode = audio_mode;
    switch_mutex_unlock(manager->mutex);
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "Dialplan Manager: audio mode changed to %s\n",
                     audio_mode == AUDIO_MODE_SILENCE ? "SILENCE" :
                     audio_mode == AUDIO_MODE_RINGBACK ? "RINGBACK" : "MUSIC");
    
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t dialplan_manager_set_auto_answer(dialplan_manager_t *manager, switch_bool_t enabled)
{
    if (!manager) {
        return SWITCH_STATUS_FALSE;
    }
    
    switch_mutex_lock(manager->mutex);
    manager->auto_answer = enabled;
    switch_mutex_unlock(manager->mutex);
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "Dialplan Manager: auto_answer changed to %s\n",
                     enabled ? "ENABLED" : "DISABLED");
    
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t dialplan_manager_set_music_class(dialplan_manager_t *manager, const char *music_class)
{
    if (!manager || zstr(music_class)) {
        return SWITCH_STATUS_FALSE;
    }
    
    switch_mutex_lock(manager->mutex);
    manager->music_class = switch_core_strdup(manager->pool, music_class);
    switch_mutex_unlock(manager->mutex);
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "Dialplan Manager: music class changed to %s\n", music_class);
    
    return SWITCH_STATUS_SUCCESS;
}

void dialplan_manager_get_status(dialplan_manager_t *manager, switch_stream_handle_t *stream)
{
    if (!manager || !stream) {
        return;
    }
    
    switch_mutex_lock(manager->mutex);
    
    stream->write_function(stream,
        "Dialplan Manager Status:\n"
        "  Mode: %s\n"
        "  Audio Mode: %s\n"
        "  Auto Answer: %s\n"
        "  Context: %s\n"
        "  Music Class: %s\n"
        "  Calls Intercepted: %u\n"
        "  Calls Parked: %u\n",
        manager->mode == DIALPLAN_MODE_PARK ? "PARK" : "DISABLED",
        manager->audio_mode == AUDIO_MODE_SILENCE ? "SILENCE" :
        manager->audio_mode == AUDIO_MODE_RINGBACK ? "RINGBACK" : "MUSIC",
        manager->auto_answer ? "YES" : "NO",
        manager->context_name,
        manager->music_class,
        manager->calls_intercepted,
        manager->calls_parked
    );
    
    switch_mutex_unlock(manager->mutex);
}
