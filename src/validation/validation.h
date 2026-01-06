#ifndef VALIDATION_H
#define VALIDATION_H

#include <stddef.h>
#include <stdint.h>

#include <cjson/cJSON.h>

typedef struct {
    size_t min;
    size_t max;
    uint8_t has_min;
    uint8_t has_max;
} v_len_rule_t;

typedef struct {
    int64_t min;
    int64_t max;
    uint8_t has_min;
    uint8_t has_max;
} v_range_rule_t;

#define v_len(min_val, max_val) \
    ((v_len_rule_t){ .min = (size_t)(min_val), .max = (size_t)(max_val), .has_min = 1, .has_max = 1 })

#define v_len_min(min_val) \
    ((v_len_rule_t){ .min = (size_t)(min_val), .has_min = 1, .has_max = 0 })

#define v_len_max(max_val) \
    ((v_len_rule_t){ .max = (size_t)(max_val), .has_min = 0, .has_max = 1 })

#define v_len_any() \
    ((v_len_rule_t){ 0 })

#define v_range(min_val, max_val) \
    ((v_range_rule_t){ .min = (int64_t)(min_val), .max = (int64_t)(max_val), .has_min = 1, .has_max = 1 })

#define v_range_min(min_val) \
    ((v_range_rule_t){ .min = (int64_t)(min_val), .has_min = 1, .has_max = 0 })

#define v_range_max(max_val) \
    ((v_range_rule_t){ .max = (int64_t)(max_val), .has_min = 0, .has_max = 1 })

#define v_range_any() \
    ((v_range_rule_t){ 0 })

const char *validation_string(
    cJSON *payload,
    const char *key,
    char *dst,
    size_t dst_size,
    v_len_rule_t limits,
    const char *message,
    int optional
);

const char *validation_number(
    cJSON *payload,
    const char *key,
    void *dst,
    size_t dst_size,
    v_range_rule_t range,
    const char *message,
    int optional
);

const char *validation_bool(
    cJSON *payload,
    const char *key,
    void *dst,
    size_t dst_size,
    const char *message,
    int optional
);

const char *validation_enum(
    cJSON *payload,
    const char *key,
    char *dst,
    size_t dst_size,
    const char *const *values,
    size_t value_count,
    const char *message,
    int optional
);

#define v_string(payload_json, target_ptr, field, limits, message) \
    validation_string((payload_json), #field, (target_ptr)->field, sizeof((target_ptr)->field), (limits), (message), 0)

#define v_string_opt(payload_json, target_ptr, field, limits, message) \
    validation_string((payload_json), #field, (target_ptr)->field, sizeof((target_ptr)->field), (limits), (message), 1)

#define v_number(payload_json, target_ptr, field, range, message) \
    validation_number((payload_json), #field, &(target_ptr)->field, sizeof((target_ptr)->field), (range), (message), 0)

#define v_number_opt(payload_json, target_ptr, field, range, message) \
    validation_number((payload_json), #field, &(target_ptr)->field, sizeof((target_ptr)->field), (range), (message), 1)

#define v_bool(payload_json, target_ptr, field, message) \
    validation_bool((payload_json), #field, &(target_ptr)->field, sizeof((target_ptr)->field), (message), 0)

#define v_bool_opt(payload_json, target_ptr, field, message) \
    validation_bool((payload_json), #field, &(target_ptr)->field, sizeof((target_ptr)->field), (message), 1)

#define v_enum(payload_json, target_ptr, field, message, ...) \
    validation_enum((payload_json), #field, (target_ptr)->field, sizeof((target_ptr)->field), \
        (const char *const[]){ __VA_ARGS__ }, \
        (size_t)(sizeof((const char *[]){ __VA_ARGS__ }) / sizeof(const char *)), \
        (message), 0)

#define v_enum_opt(payload_json, target_ptr, field, message, ...) \
    validation_enum((payload_json), #field, (target_ptr)->field, sizeof((target_ptr)->field), \
        (const char *const[]){ __VA_ARGS__ }, \
        (size_t)(sizeof((const char *[]){ __VA_ARGS__ }) / sizeof(const char *)), \
        (message), 1)

#endif /* VALIDATION_H */
