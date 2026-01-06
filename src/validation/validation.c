#include "validation.h"

#include <string.h>

static const char *ERR_MISSING_OR_INVALID = "missing or invalid field";
static const char *ERR_EXPECTED_STRING    = "expected string";
static const char *ERR_EXPECTED_NUMBER    = "expected number";
static const char *ERR_EXPECTED_BOOL      = "expected boolean";
static const char *ERR_INVALID_ENUM       = "invalid value";

static inline const char *validation_error(const char *preferred, const char *fallback) {
    return preferred ? preferred : fallback;
}

static inline void store_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static inline void store_number(void *dst, size_t size, int64_t value) {
    if (!dst || size == 0) {
        return;
    }

    if (size == sizeof(int32_t)) {
        *(int32_t *)dst = (int32_t)value;
        return;
    }

    if (size == sizeof(int64_t)) {
        *(int64_t *)dst = (int64_t)value;
        return;
    }

    if (size == sizeof(double)) {
        *(double *)dst = (double)value;
        return;
    }

    if (size >= sizeof(int64_t)) {
        *(int64_t *)dst = (int64_t)value;
        return;
    }

    if (size >= sizeof(int32_t)) {
        *(int32_t *)dst = (int32_t)value;
        return;
    }

    *(int8_t *)dst = (int8_t)value;
}

static inline void store_bool(void *dst, size_t size, int truthy) {
    if (!dst || size == 0) {
        return;
    }

    uint8_t value = truthy ? 1u : 0u;

    if (size == sizeof(uint8_t)) {
        *(uint8_t *)dst = value;
        return;
    }

    if (size == sizeof(uint32_t)) {
        *(uint32_t *)dst = (uint32_t)value;
        return;
    }

    if (size == sizeof(uint64_t)) {
        *(uint64_t *)dst = (uint64_t)value;
        return;
    }

    *(uint8_t *)dst = value;
}

const char *validation_string(
    cJSON *payload,
    const char *key,
    char *dst,
    size_t dst_size,
    v_len_rule_t limits,
    const char *message,
    int optional
) {
    if (!payload || !key || !dst) {
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(payload, key);
    if (!item) {
        if (optional) {
            if (dst_size > 0) {
                dst[0] = '\0';
            }
            return NULL;
        }
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    if (!cJSON_IsString(item) || !item->valuestring) {
        return validation_error(message, ERR_EXPECTED_STRING);
    }

    const char *input = item->valuestring;
    size_t len = strlen(input);

    if (limits.has_min && len < limits.min) {
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    if (limits.has_max && len > limits.max) {
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    store_string(dst, dst_size, input);
    return NULL;
}

const char *validation_number(
    cJSON *payload,
    const char *key,
    void *dst,
    size_t dst_size,
    v_range_rule_t range,
    const char *message,
    int optional
) {
    if (!payload || !key || !dst) {
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(payload, key);
    if (!item) {
        if (optional) {
            return NULL;
        }
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    if (!cJSON_IsNumber(item)) {
        return validation_error(message, ERR_EXPECTED_NUMBER);
    }

    int64_t value = (int64_t)item->valuedouble;

    if (range.has_min && value < range.min) {
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    if (range.has_max && value > range.max) {
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    store_number(dst, dst_size, value);
    return NULL;
}

const char *validation_bool(
    cJSON *payload,
    const char *key,
    void *dst,
    size_t dst_size,
    const char *message,
    int optional
) {
    if (!payload || !key || !dst) {
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(payload, key);
    if (!item) {
        if (optional) {
            return NULL;
        }
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    if (!cJSON_IsBool(item)) {
        return validation_error(message, ERR_EXPECTED_BOOL);
    }

    store_bool(dst, dst_size, cJSON_IsTrue(item));
    return NULL;
}

const char *validation_enum(
    cJSON *payload,
    const char *key,
    char *dst,
    size_t dst_size,
    const char *const *values,
    size_t value_count,
    const char *message,
    int optional
) {
    if (!payload || !key || !dst || !values || value_count == 0) {
        return validation_error(message, ERR_INVALID_ENUM);
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(payload, key);
    if (!item) {
        if (optional) {
            if (dst_size > 0) {
                dst[0] = '\0';
            }
            return NULL;
        }
        return validation_error(message, ERR_MISSING_OR_INVALID);
    }

    if (!cJSON_IsString(item) || !item->valuestring) {
        return validation_error(message, ERR_EXPECTED_STRING);
    }

    const char *input = item->valuestring;
    for (size_t i = 0; i < value_count; i++) {
        const char *candidate = values[i];
        if (candidate && strcmp(candidate, input) == 0) {
            store_string(dst, dst_size, input);
            return NULL;
        }
    }

    return validation_error(message, ERR_INVALID_ENUM);
}
