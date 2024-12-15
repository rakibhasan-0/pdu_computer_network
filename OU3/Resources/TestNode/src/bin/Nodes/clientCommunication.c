#include "clientCommunication.h"

/*
 * q9_state - Handles value operations (insertion, removal, lookup)
 *
 * This state is responsible for processing incoming PDUs related to value operations.
 * It handles the following types of PDUs:
 * - VAL_INSERT: Insert a value into the node's data structure.
 * - VAL_REMOVE: Remove a value from the node's data structure.
 * - VAL_LOOKUP: Lookup a value in the node's data structure.
 *
 * Steps:
 * 1. Receive the PDU from the predecessor or successor.
 * 2. Determine the type of PDU and process accordingly.
 * 3. Perform the necessary operations on the node's data structure.
 * 4. Transition back to Q6 state after processing the PDU.
 *
 * Parameters:
 * - n: Pointer to the Node structure.
 * - data: Additional data passed to the state (if any).
 *
 * Returns:
 * - 0 on success.
 * - 1 on failure.
 */


int q9_state(void* n, void* data) {
    Node* node = (Node*)n;
    printf("[Q9 state]\n");

    char* buffer = (char*)data;
    uint8_t pdu_type = buffer[0]; // Assuming the first byte indicates the PDU type
    printf("PDU type: %d\n", pdu_type);

    switch (pdu_type) {
        case VAL_INSERT:
            printf("Handling VAL_INSERT\n");
            struct VAL_INSERT_PDU* val_insert_pdu = (struct VAL_INSERT_PDU*)buffer;
            printf("SSN: %s\n", val_insert_pdu->ssn);
            printf("Name Length: %d\n", val_insert_pdu->name_length);
            printf("Email Length: %d\n", val_insert_pdu->email_length);
            handle_val_insert(node, val_insert_pdu);
            break;
        case VAL_REMOVE:
            printf("Handling VAL_REMOVE\n");
            struct VAL_REMOVE_PDU* val_remove_pdu = (struct VAL_REMOVE_PDU*)buffer;
            printf("SSN: %s\n", val_remove_pdu->ssn);
            handle_val_remove(node, val_remove_pdu);
            break;
        case VAL_LOOKUP:
            printf("Handling VAL_LOOKUP\n");
            struct VAL_LOOKUP_PDU* val_lookup_pdu = (struct VAL_LOOKUP_PDU*)buffer;
            printf("SSN: %s\n", val_lookup_pdu->ssn);
            handle_val_lookup(node, val_lookup_pdu);
            break;
        default:
            printf("Unknown PDU type: %d\n", pdu_type);
            break;
    }

    // Transition back to Q6 state after processing the PDU
    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}

void handle_val_insert(Node* node, struct VAL_INSERT_PDU* pdu) {
    printf("Inserting value with SSN: %s\n", pdu->ssn);
}

void handle_val_remove(Node* node, struct VAL_REMOVE_PDU* pdu) {
    printf("Removing value with SSN: %s\n", pdu->ssn);
}

void handle_val_lookup(Node* node, struct VAL_LOOKUP_PDU* pdu) {
    printf("Looking up value with SSN: %s\n", pdu->ssn);
}
