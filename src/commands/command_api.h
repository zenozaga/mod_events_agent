#ifndef COMMAND_API_H
#define COMMAND_API_H

#include "../driver_interface.h"

void handle_api_generic(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data);
switch_status_t command_api_register(event_driver_t *driver);

#endif
