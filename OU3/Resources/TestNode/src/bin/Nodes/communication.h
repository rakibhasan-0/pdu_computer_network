#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "node.h"
#include "join_network.h"
#include "controller.h"
#include "utils.h"
#include "signal_handler.h"   
#include "initialization.h"



// Function pointer for state handler
typedef int (*state_handler_t)(void *node, void *data);

// State handler functions for each state
int q5_state(void *n, void *data);
int q12_state(void *n, void *data);
//int q9_state(void* n, void* data);
int q14_state(void *n, void *data);
int q13_state(void *n, void *data);
int q17_state(void *n, void *data);

//void handle_val_insert(Node* node, struct VAL_INSERT_PDU* pdu);
//void handle_val_remove(Node* node, struct VAL_REMOVE_PDU* pdu);
//void handle_val_lookup(Node* node, struct VAL_LOOKUP_PDU* pdu);
// Array of state handler functions indexed by state constants
extern state_handler_t state_handlers[];

#endif // COMMUNICATION_H
