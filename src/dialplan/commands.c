#include "commands.h"
#include "manager.h"
#include "../commands/core.h"
#include "validation/validation.h"

// ============================
// Dialplan Command Handlers
// ============================

static dialplan_manager_t *g_dialplan_manager = NULL;

// ---------- Helpers ----------

static command_result_t require_manager(void) {
    if (!g_dialplan_manager) {
        return command_result_error("Dialplan manager not initialized");
    }
    return command_result_ok();
}

// ============================
// dialplan.enable
// ============================

static command_result_t dialplan_enable(const command_request_t *request) {
    command_result_t guard = require_manager();
    if (guard.error) {
        return guard;
    }

    if (dialplan_manager_set_mode(g_dialplan_manager, DIALPLAN_MODE_PARK) != SWITCH_STATUS_SUCCESS) {
        return command_result_error("Failed to enable park mode");
    }

    cJSON *data = cJSON_CreateObject();
    if (data) {
        cJSON_AddStringToObject(data, "mode", "park");
    }

    command_result_t result = command_result_ok();
    result.message = "Park mode enabled";
    result.data = data;
    return result;
}

// ============================
// dialplan.disable
// ============================

static command_result_t dialplan_disable(const command_request_t *request) {
    command_result_t guard = require_manager();
    if (guard.error) {
        return guard;
    }

    if (dialplan_manager_set_mode(g_dialplan_manager, DIALPLAN_MODE_DISABLED) != SWITCH_STATUS_SUCCESS) {
        return command_result_error("Failed to disable park mode");
    }

    cJSON *data = cJSON_CreateObject();
    if (data) {
        cJSON_AddStringToObject(data, "mode", "disabled");
    }

    command_result_t result = command_result_ok();
    result.message = "Park mode disabled";
    result.data = data;
    return result;
}

// ============================
// dialplan.audio
// ============================

// Payload + Schema

typedef struct {
    char mode[8];
    char music_class[64];
} dialplan_audio_payload_t;

static const char *validate_audio_payload(cJSON *json, dialplan_audio_payload_t *payload) {
    const char *err = v_enum(json, payload, mode,
                             "Invalid mode. Use silence, ringback, or music",
                             "silence", "ringback", "music");
    if (err) {
        return err;
    }

    err = v_string_opt(json, payload, music_class,
                       v_len_max(63),
                       "music_class must be 63 characters or fewer");
    return err;
}

static command_result_t dialplan_audio(const command_request_t *request) {
    command_result_t guard = require_manager();
    if (guard.error) {
        return guard;
    }

    dialplan_audio_payload_t payload = {0};
    const char *validation_error = validate_audio_payload(request->payload, &payload);
    if (validation_error) {
        return command_result_error(validation_error);
    }

    const char *mode_str = payload.mode;
    audio_mode_t audio_mode;

    if (!strcmp(mode_str, "silence")) {
        audio_mode = AUDIO_MODE_SILENCE;
    } else if (!strcmp(mode_str, "ringback")) {
        audio_mode = AUDIO_MODE_RINGBACK;
    } else if (!strcmp(mode_str, "music")) {
        audio_mode = AUDIO_MODE_MUSIC;
        if (!switch_strlen_zero(payload.music_class)) {
            dialplan_manager_set_music_class(g_dialplan_manager, payload.music_class);
        }
    } else {
        return command_result_error("Invalid mode. Use: silence, ringback, or music");
    }

    if (dialplan_manager_set_audio_mode(g_dialplan_manager, audio_mode) != SWITCH_STATUS_SUCCESS) {
        return command_result_error("Failed to set audio mode");
    }

    cJSON *data = cJSON_CreateObject();
    if (data) {
        cJSON_AddStringToObject(data, "mode", mode_str);
    }

    command_result_t result = command_result_ok();
    result.message = "Audio mode updated";
    result.data = data;
    return result;
}

// ============================
// dialplan.autoanswer
// ============================


typedef struct {
    uint8_t enabled;
} dialplan_autoanswer_payload_t;

static const char *validate_autoanswer_payload(cJSON *json, dialplan_autoanswer_payload_t *payload) {
    return v_bool(json, payload, enabled, "enabled must be a boolean flag");
}

static command_result_t dialplan_autoanswer(const command_request_t *request) {
    command_result_t guard = require_manager();
    if (guard.error) {
        return guard;
    }

    dialplan_autoanswer_payload_t payload = {0};
    const char *validation_error = validate_autoanswer_payload(request->payload, &payload);
    if (validation_error) {
        return command_result_error(validation_error);
    }

    const switch_bool_t enabled = payload.enabled ? SWITCH_TRUE : SWITCH_FALSE;
    if (dialplan_manager_set_auto_answer(g_dialplan_manager, enabled) != SWITCH_STATUS_SUCCESS) {
        return command_result_error("Failed to set auto-answer");
    }

    cJSON *data = cJSON_CreateObject();
    if (data) {
        cJSON_AddBoolToObject(data, "enabled", enabled);
    }

    command_result_t result = command_result_ok();
    result.message = "Auto-answer updated";
    result.data = data;
    return result;
}

// ============================
// dialplan.status
// ============================

static command_result_t dialplan_status(const command_request_t *request) {
    command_result_t guard = require_manager();
    if (guard.error) {
        return guard;
    }

    switch_stream_handle_t stream = {0};
    SWITCH_STANDARD_STREAM(stream);
    dialplan_manager_get_status(g_dialplan_manager, &stream);

    cJSON *data = cJSON_CreateObject();
    if (data && stream.data) {
        cJSON_AddStringToObject(data, "info", stream.data);
    }

    switch_safe_free(stream.data);

    command_result_t result = command_result_ok();
    result.message = "Dialplan status retrieved";
    result.data = data;
    return result;
}

switch_status_t command_dialplan_init(dialplan_manager_t *manager) {
    if (!manager) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[mod_event_agent] Dialplan manager unavailable; dialplan commands disabled");
        return SWITCH_STATUS_FALSE;
    }

    g_dialplan_manager = manager;

    if (command_register_handler("dialplan.enable", dialplan_enable) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }
    if (command_register_handler("dialplan.disable", dialplan_disable) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }
    if (command_register_handler("dialplan.audio", dialplan_audio) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }
    if (command_register_handler("dialplan.autoanswer", dialplan_autoanswer) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }
    if (command_register_handler("dialplan.status", dialplan_status) != SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_FALSE;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[mod_event_agent] Dialplan commands registered (enable/disable/audio/autoanswer/status)");
    return SWITCH_STATUS_SUCCESS;
}

void command_dialplan_shutdown(void) {
    g_dialplan_manager = NULL;
}
