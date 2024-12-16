#ifndef CLIENTCOMMUNICATION_H
#define CLIENTCOMMUNICATION_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "node.h"
#include "hashtable.h"

struct Entry {
    char ssn[12]; 
    char name[99]; // Adjust size as needed
    char email[99]; // Adjust size as needed
    struct Entry* next;
};

// State handler functions for each state
int q9_state(void* n, void* data);

void handle_val_insert(Node* node, struct VAL_INSERT_PDU* pdu);
void handle_val_remove(Node* node, struct VAL_REMOVE_PDU* pdu);
void handle_val_lookup(Node* node, struct VAL_LOOKUP_PDU* pdu);
bool in_my_range(Node* node, const char* ssn);

extern state_handler_t state_handlers[];

#endif 
