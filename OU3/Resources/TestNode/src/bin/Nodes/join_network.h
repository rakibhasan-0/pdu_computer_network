// here shall we place the states that have used for the initialisation of the node.
// Q4, Q7 and Q8.

#ifndef JOIN_NETWORK_H
#define JOIN_NETWORK_H
#include "node.h"
#include "communication.h"
#include "controller.h"
#include "utils.h"

// State handler functions for each state
int q4_state(void *n, void *data);
int q7_state(void *n, void *data);
int q8_state(void *n, void *data);

#endif // JOIN_NETWORK_H