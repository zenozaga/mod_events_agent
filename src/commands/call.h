#ifndef COMMAND_CALL_H
#define COMMAND_CALL_H

#include "../drivers/interface.h"

void handle_originate(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data);
void handle_hangup(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data);
void handle_originate_async(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data);
void handle_hangup_async(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data);

switch_status_t command_call_register(event_driver_t *driver);

#endif
