#ifndef COMMAND_CORE_H
#define COMMAND_CORE_H

#include "../mod_event_agent.h"
#include <cjson/cJSON.h>

typedef struct {
    uint64_t requests_received;
    uint64_t requests_success;
    uint64_t requests_failed;
} command_stats_t;

typedef struct command_request {
    cJSON *payload;
    const char *command;
    const char *subject;
    const char *reply_to;
    switch_bool_t async;
} command_request_t;

typedef struct command_result {
    cJSON *data;
    char *error;
    const char *message;
} command_result_t;

typedef command_result_t (*command_handler_fn)(const command_request_t *request);

switch_bool_t should_process_request(cJSON *json);
cJSON* build_json_response_object(switch_bool_t success, const char *message);
char* build_json_response(switch_bool_t success, const char *message, const char *data);
uint64_t command_current_timestamp_us(void);
void command_stats_increment_received(void);
void command_stats_increment_success(void);
void command_stats_increment_failed(void);
void command_stats_get(uint64_t *requests, uint64_t *success, uint64_t *failed);

command_result_t command_result_ok(void);
command_result_t command_result_from_string(const char *value);
command_result_t command_result_error(const char *message);
void command_result_free(command_result_t *result);

switch_status_t command_register_handler(const char *name, command_handler_fn handler);
void command_register_default_handler(command_handler_fn handler);

#endif
