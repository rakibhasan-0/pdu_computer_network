#ifndef INITIALIZATION_H
#define INITIALIZATION_H

#include "node.h"
#include "communication.h"
#include "join_network.h"
#include "controller.h" 
#include "signal_handler.h"
#include "utils.h"


// Function prototypes for all state functions
int q1_state(void* n, void* data);
int q2_state(void* n, void* data);
int q3_state(void* n, void* data);

// Add more states here, depending on the design

// Declare state_handlers array
extern int (*state_handlers[])(void*, void*);

#endif
