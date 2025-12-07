#ifndef DIALPLAN_MANAGER_H
#define DIALPLAN_MANAGER_H

#include <switch.h>

/* Forward declaration if needed elsewhere */
typedef struct dialplan_manager_s dialplan_manager_t;

typedef enum {
    DIALPLAN_MODE_DISABLED,    /* No park, normal dialplan */
    DIALPLAN_MODE_PARK,        /* Park all inbound calls */
} dialplan_mode_t;

typedef enum {
    AUDIO_MODE_SILENCE,        /* Silent - no audio */
    AUDIO_MODE_RINGBACK,       /* Ring back tone */
    AUDIO_MODE_MUSIC,          /* Music on hold */
} audio_mode_t;

struct dialplan_manager_s {
    switch_memory_pool_t *pool;
    switch_mutex_t *mutex;
    
    /* Configuration */
    dialplan_mode_t mode;
    audio_mode_t audio_mode;
    switch_bool_t auto_answer;
    char *context_name;
    char *music_class;  /* MOH class to use */
    
    /* XML binding */
    switch_xml_binding_t *binding;
    
    /* Statistics */
    uint32_t calls_intercepted;
    uint32_t calls_parked;
    
};

/* Initialize dialplan manager */
switch_status_t dialplan_manager_init(dialplan_manager_t **manager, switch_memory_pool_t *pool);

/* Shutdown dialplan manager */
void dialplan_manager_shutdown(dialplan_manager_t *manager);

/* Set park mode */
switch_status_t dialplan_manager_set_mode(dialplan_manager_t *manager, dialplan_mode_t mode);

/* Set audio mode */
switch_status_t dialplan_manager_set_audio_mode(dialplan_manager_t *manager, audio_mode_t audio_mode);

/* Set auto answer */
switch_status_t dialplan_manager_set_auto_answer(dialplan_manager_t *manager, switch_bool_t enabled);

/* Set music class */
switch_status_t dialplan_manager_set_music_class(dialplan_manager_t *manager, const char *music_class);

/* Get current status */
void dialplan_manager_get_status(dialplan_manager_t *manager, switch_stream_handle_t *stream);

#endif /* DIALPLAN_MANAGER_H */
