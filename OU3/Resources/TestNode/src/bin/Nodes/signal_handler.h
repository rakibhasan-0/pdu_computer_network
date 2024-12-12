#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include "node.h"
#include "communication.h"
#include "join_network.h"
#include "controller.h"
#include "utils.h"
#include "initialization.h"



int q10_state(void* n, void* data);
void register_signal_handlers();
int q11_state(void* n, void* data);
int q15_state(void* n, void* data);
int q18_state(void* n, void* data);
int q16_state(void* n, void* data);

#endif
