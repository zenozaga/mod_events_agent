#ifndef DIALPLAN_COMMANDS_H
#define DIALPLAN_COMMANDS_H

#include <switch.h>
#include "manager.h"

switch_status_t command_dialplan_init(dialplan_manager_t *manager);
void command_dialplan_shutdown(void);

#endif /* DIALPLAN_COMMANDS_H */
