#ifndef CLIENTCOMMUNICATION_H
#define CLIENTCOMMUNICATION_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "node.h"
#define MAX_NAME_LENGTH 255
#define MAX_EMAIL_LENGTH 255


typedef struct Node Node;

struct Entry {
    char* ssn; 
    char* name; 
    char* email;
    size_t name_length;
    size_t email_length;
    //struct Entry* next; // we maynot need this as we will use the hash table to store the entries.
};

typedef struct Entry Entry;

// State handler functions for each state
int q9_state(void* n, void* data);

//void handle_val_insert(Node* node, struct VAL_INSERT_PDU* pdu);
//void handle_val_remove(Node* node, struct VAL_REMOVE_PDU* pdu);
//void handle_val_lookup(Node* node, struct VAL_LOOKUP_PDU* pdu);
//bool in_my_range(Node* node, const char* ssn);
//void free_entry(void* entry); // we will need this for the free the memory of the entry.


#endif 
