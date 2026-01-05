#include "commands.h"
#include "manager.h"
#include "../mod_event_agent.h"
#include "../commands/core.h"
#include <switch.h>
#include <string.h>

static dialplan_manager_t *g_dialplan_manager = NULL;
static event_driver_t *g_driver = NULL;

static const char *get_subject_prefix(void)
{
    extern mod_event_agent_globals_t globals;
    return (globals.subject_prefix && *globals.subject_prefix) ? globals.subject_prefix : DEFAULT_SUBJECT_PREFIX;
}

static void build_subject(char *buffer, size_t len, const char *suffix)
{
    switch_snprintf(buffer, len, "%s.%s", get_subject_prefix(), suffix);
}

static void publish_json_response(cJSON *response, const char *reply_subject)
{
    if (!response) {
        return;
    }

    char *json_str = cJSON_PrintUnformatted(response);
    if (json_str && g_driver && reply_subject) {
        g_driver->publish(g_driver, reply_subject, json_str, strlen(json_str));
    }
    switch_safe_free(json_str);
    cJSON_Delete(response);
}

static void publish_simple_response(const char *reply_subject, switch_bool_t success, const char *message)
{
    cJSON *response = build_json_response_object(success, message);
    publish_json_response(response, reply_subject);
}

static void handle_dialplan_enable(const char *subject, const char *data, size_t data_len, const char *reply_subject, void *user_data)
{
    switch_bool_t success = SWITCH_FALSE;
    const char *message = NULL;

    if (!g_dialplan_manager) {
        message = "Dialplan manager not initialized";
    } else if (dialplan_manager_set_mode(g_dialplan_manager, DIALPLAN_MODE_PARK) == SWITCH_STATUS_SUCCESS) {
        success = SWITCH_TRUE;
        message = "Park mode enabled";
    } else {
        message = "Failed to enable park mode";
    }

    cJSON *response = build_json_response_object(success, message);
    if (response && success) {
        cJSON_AddStringToObject(response, "mode", "park");
    }

    publish_json_response(response, reply_subject);
}

static void handle_dialplan_disable(const char *subject, const char *data, size_t data_len, const char *reply_subject, void *user_data)
{
    switch_bool_t success = SWITCH_FALSE;
    const char *message = NULL;

    if (!g_dialplan_manager) {
        message = "Dialplan manager not initialized";
    } else if (dialplan_manager_set_mode(g_dialplan_manager, DIALPLAN_MODE_DISABLED) == SWITCH_STATUS_SUCCESS) {
        success = SWITCH_TRUE;
        message = "Park mode disabled";
    } else {
        message = "Failed to disable park mode";
    }

    cJSON *response = build_json_response_object(success, message);
    if (response && success) {
        cJSON_AddStringToObject(response, "mode", "disabled");
    }

    publish_json_response(response, reply_subject);
}

static void handle_dialplan_audio(const char *subject, const char *data, size_t data_len, const char *reply_subject, void *user_data)
{
    cJSON *request = NULL;
    switch_bool_t success = SWITCH_FALSE;
    const char *message = NULL;
    const char *mode_str = NULL;

    if (!g_dialplan_manager) {
        message = "Dialplan manager not initialized";
        goto publish;
    }

    request = cJSON_Parse(data);
    if (!request) {
        message = "Invalid JSON payload";
        goto publish;
    }

    cJSON *mode_obj = cJSON_GetObjectItem(request, "mode");
    if (!mode_obj || !cJSON_IsString(mode_obj)) {
        message = "Missing 'mode' field";
        goto publish;
    }

    mode_str = mode_obj->valuestring;
    audio_mode_t audio_mode;

    if (strcmp(mode_str, "silence") == 0) {
        audio_mode = AUDIO_MODE_SILENCE;
    } else if (strcmp(mode_str, "ringback") == 0) {
        audio_mode = AUDIO_MODE_RINGBACK;
    } else if (strcmp(mode_str, "music") == 0) {
        audio_mode = AUDIO_MODE_MUSIC;

        cJSON *music_class_obj = cJSON_GetObjectItem(request, "music_class");
        if (music_class_obj && cJSON_IsString(music_class_obj)) {
            dialplan_manager_set_music_class(g_dialplan_manager, music_class_obj->valuestring);
        }
    } else {
        message = "Invalid mode. Use: silence, ringback, or music";
        goto publish;
    }

    if (dialplan_manager_set_audio_mode(g_dialplan_manager, audio_mode) == SWITCH_STATUS_SUCCESS) {
        success = SWITCH_TRUE;
        message = "Audio mode updated";
    } else {
        message = "Failed to set audio mode";
    }

publish:
    cJSON *response = build_json_response_object(success, message ? message : "Audio mode response");
    if (response && mode_str) {
        cJSON_AddStringToObject(response, "mode", mode_str);
    }

    publish_json_response(response, reply_subject);

    if (request) {
        cJSON_Delete(request);
    }
}

static void handle_dialplan_autoanswer(const char *subject, const char *data, size_t data_len, const char *reply_subject, void *user_data)
{
    cJSON *request = NULL;
    switch_bool_t success = SWITCH_FALSE;
    const char *message = NULL;
    switch_bool_t enabled = SWITCH_FALSE;
    switch_bool_t has_enabled_value = SWITCH_FALSE;

    if (!g_dialplan_manager) {
        message = "Dialplan manager not initialized";
        goto publish;
    }

    request = cJSON_Parse(data);
    if (!request) {
        message = "Invalid JSON payload";
        goto publish;
    }

    cJSON *enabled_obj = cJSON_GetObjectItem(request, "enabled");
    if (!enabled_obj || !cJSON_IsBool(enabled_obj)) {
        message = "Missing 'enabled' boolean field";
        goto publish;
    }

    enabled = cJSON_IsTrue(enabled_obj) ? SWITCH_TRUE : SWITCH_FALSE;
    has_enabled_value = SWITCH_TRUE;

    if (dialplan_manager_set_auto_answer(g_dialplan_manager, enabled) == SWITCH_STATUS_SUCCESS) {
        success = SWITCH_TRUE;
        message = "Auto-answer updated";
    } else {
        message = "Failed to set auto-answer";
    }

publish:
    cJSON *response = build_json_response_object(success, message ? message : "Auto-answer response");
    if (response && has_enabled_value) {
        cJSON_AddBoolToObject(response, "enabled", enabled);
    }

    publish_json_response(response, reply_subject);

    if (request) {
        cJSON_Delete(request);
    }
}

static void handle_dialplan_status(const char *subject, const char *data, size_t data_len, const char *reply_subject, void *user_data)
{
    switch_bool_t success = SWITCH_FALSE;
    const char *message = NULL;
    switch_stream_handle_t stream = { 0 };

    if (!g_dialplan_manager) {
        message = "Dialplan manager not initialized";
    } else {
        SWITCH_STANDARD_STREAM(stream);
        dialplan_manager_get_status(g_dialplan_manager, &stream);
        success = SWITCH_TRUE;
        message = "Dialplan status retrieved";
    }

    cJSON *response = build_json_response_object(success, message ? message : "Dialplan status");
    if (response && success && stream.data) {
        cJSON_AddStringToObject(response, "info", stream.data);
    }

    publish_json_response(response, reply_subject);
    switch_safe_free(stream.data);
}

static void dispatch_dialplan_command(const char *subject, const char *data, size_t data_len, const char *reply_subject, void *user_data)
{
    const char *prefix = get_subject_prefix();
    size_t prefix_len = strlen(prefix);

    if (strncmp(subject, prefix, prefix_len) != 0 || subject[prefix_len] != '.') {
        publish_simple_response(reply_subject, SWITCH_FALSE, "Invalid subject prefix");
        return;
    }

    const char *suffix = subject + prefix_len + 1; /* skip dot */
    static const char *dialplan_prefix = "cmd.dialplan.";
    size_t dialplan_len = strlen(dialplan_prefix);

    if (strncmp(suffix, dialplan_prefix, dialplan_len) != 0) {
        /* Not a dialplan command; let other handlers manage it */
        return;
    }

    const char *command = suffix + dialplan_len;

    if (strcmp(command, "enable") == 0) {
        handle_dialplan_enable(subject, data, data_len, reply_subject, user_data);
    } else if (strcmp(command, "disable") == 0) {
        handle_dialplan_disable(subject, data, data_len, reply_subject, user_data);
    } else if (strcmp(command, "audio") == 0) {
        handle_dialplan_audio(subject, data, data_len, reply_subject, user_data);
    } else if (strcmp(command, "autoanswer") == 0) {
        handle_dialplan_autoanswer(subject, data, data_len, reply_subject, user_data);
    } else if (strcmp(command, "status") == 0) {
        handle_dialplan_status(subject, data, data_len, reply_subject, user_data);
    } else {
        publish_simple_response(reply_subject, SWITCH_FALSE, "Unknown dialplan command");
    }
}

switch_status_t command_dialplan_init(event_driver_t *driver, dialplan_manager_t *manager)
{
    if (!driver || !manager) {
        return SWITCH_STATUS_FALSE;
    }
    
    g_driver = driver;
    g_dialplan_manager = manager;
    
    /* Subscribe to dialplan commands */
    char subject[256];

    build_subject(subject, sizeof(subject), ">");
    driver->subscribe(driver, subject, dispatch_dialplan_command, NULL);
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "Dialplan command handlers initialized\n");
    
    return SWITCH_STATUS_SUCCESS;
}

void command_dialplan_shutdown(event_driver_t *driver)
{
    if (!driver) {
        return;
    }
    
    /* Unsubscribe from dialplan commands */
    char subject[256];

    build_subject(subject, sizeof(subject), ">");
    driver->unsubscribe(driver, subject);
    
    g_driver = NULL;
    g_dialplan_manager = NULL;
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
                     "Dialplan command handlers shutdown\n");
}
