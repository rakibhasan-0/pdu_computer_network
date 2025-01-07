#include "client_communication.h"

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

static void insertion_of_value(Node* node, struct VAL_INSERT_PDU* pdu);
static bool parse_val_insert_pdu(const uint8_t* buffer, struct VAL_INSERT_PDU* pdu_out);
static void remove_value(Node* node, struct VAL_REMOVE_PDU* val_remove_pdu);
static bool parse_val_remove_pdu(const uint8_t* buffer, struct VAL_REMOVE_PDU* pdu_out);
static void lookup_value(Node* node, struct VAL_LOOKUP_PDU* pdu);
static bool parse_val_lookup_pdu(const uint8_t* buffer, struct VAL_LOOKUP_PDU* pdu_out);
static void send_lookup_response(Node* node, struct VAL_LOOKUP_RESPONSE_PDU* response_pdu, struct sockaddr_in* client_addr);

int q9_state(void* n, void* data) {
    Node* node = (Node*)n;
    //printf("[Q9 state]\n");

    // now we will transfer data into the queue as we have inserted data into the queue in the state 6.
    
    //queue_t* q = (queue_t*)data;
    //printf("The size of the queue is %d\n", q->size);
    
    // we will dequeue the data from the queue.
    //printf("PDU type: %d\n", pdu_type);

  
    PDU* pdu = (PDU*)data;
    uint8_t pdu_type = pdu->type;

    switch (pdu_type) {

        case VAL_INSERT:
            struct VAL_INSERT_PDU pdu_insert = {0};
			printf("Current sockfd_b insert: %d\n", node->sockfd_b);
            // Parse the buffer into the PDU structure of the VAL_INSERT_PDU type
            if(parse_val_insert_pdu(pdu->buffer, &pdu_insert)) {
                insertion_of_value(node, &pdu_insert);
            }
            free(pdu_insert.name);
            free(pdu_insert.email);
            
            break;

        case VAL_REMOVE:
            printf("Handling VAL_REMOVE\n");
            struct VAL_REMOVE_PDU val_remove_pdu;
        // parse_val_remove_pdu(entry_data, &val_remove_pdu);
            printf("SSN: %s\n", val_remove_pdu.ssn);
            remove_value(node, &val_remove_pdu);
            remove_value(node, &val_remove_pdu);
            break;
        case VAL_LOOKUP: 
            printf("Handling VAL_LOOKUP\n");
			printf("Current sockfd_b lookup: %d\n", node->sockfd_b);
            struct VAL_LOOKUP_PDU val_lookup_pdu;
			if (parse_val_lookup_pdu(pdu->buffer, &val_lookup_pdu)) {
				printf("SSN: %s\n", val_lookup_pdu.ssn);
				lookup_value(node, &val_lookup_pdu);
			}
        
            break;
        default:
            printf("Unknown PDU type: %d\n", pdu_type);
            break;
    }

    

    
    
    // Transition back to Q6 state after processing the PDU
    //node->state_handler = state_handlers[STATE_6];
    //node->state_handler(node, NULL);

    return 0;
}


//this fucntion will free the entry.
// it is being passed to the ht_create function.
// it is new version for the insertaion of the node
static void insertion_of_value(Node* node, struct VAL_INSERT_PDU* pdu) {
    uint8_t hash_value = hash_ssn(pdu->ssn);

    //printf("Hash value: %d\n", hash_value);
    if (hash_value >= node->hash_range_start && hash_value <= node->hash_range_end) {
        printf("Inserting value with SSN: %.12s\n", pdu->ssn);

        Entry* entry = (Entry*)malloc(sizeof(Entry));
        if (!entry) {
            perror("Memory allocation failed for Entry");
            return;
        }

        // Copy SSN directly (assuming SSN_LENGTH is fixed and known)
        entry->ssn = malloc(SSN_LENGTH);
        if (!entry->ssn) {
            perror("Memory allocation failed for SSN");
            free(entry);
            return;
        }
        memcpy(entry->ssn, pdu->ssn, SSN_LENGTH);

        // Copy name
        entry->name = malloc(pdu->name_length);
        if (!entry->name) {
            perror("Memory allocation failed for name");
            free(entry->ssn);
            free(entry);
            return;
        }
        memcpy(entry->name, pdu->name, pdu->name_length);
        entry->name_length = pdu->name_length;

        // Copy email
        entry->email = malloc(pdu->email_length);
        if (!entry->email) {
            perror("Memory allocation failed for email");
            free(entry->name);
            free(entry->ssn);
            free(entry);
            return;
        }
        memcpy(entry->email, pdu->email, pdu->email_length);
        entry->email_length = pdu->email_length;



        //printf("we are about to insert the value\n");
        node->hash_table = ht_insert(node->hash_table, entry->ssn, entry);
        //printf("Value inserted successfully\n");
    } else {

        // we will forward the pdu to the successor.
        size_t pdu_size = 1 + SSN_LENGTH + 1 + pdu->name_length + 1 + pdu->email_length;
        uint8_t* out_buffer = malloc(pdu_size);
        if (!out_buffer) {
            perror("Memory allocation failed for PDU buffer");
            return;
        }
        size_t offset = 0;
        out_buffer[offset++] = VAL_INSERT;
        memcpy(out_buffer + offset, pdu->ssn, SSN_LENGTH);
        offset += SSN_LENGTH;

        out_buffer[offset++] = pdu->name_length;
        memcpy(out_buffer + offset, pdu->name, pdu->name_length);
        offset += pdu->name_length;

        out_buffer[offset++] = pdu->email_length;
        memcpy(out_buffer + offset, pdu->email, pdu->email_length);
        offset += pdu->email_length;

        int send_status = send(node->sockfd_b, out_buffer, pdu_size, 0);
        if(send_status == -1){
            perror("send failure");
            return;
        }
        //printf("Forwarding VAL_INSERT to successor\n");

        free(out_buffer);

        //printf("VAL_INSERT forwarded to successor\n");
    }
}

// the purpose of this function is to parse the val_insert pdu.
static bool parse_val_insert_pdu(const uint8_t* buffer, struct VAL_INSERT_PDU* pdu_out) {
    size_t offset = 0;

    //printf("Parsing PDU...\n");

    // Type (1 byte)
    pdu_out->type = buffer[offset++];
    //printf("Type: %d (offset: %zu)\n", pdu_out->type, offset);

    // SSN (12 bytes)
    memcpy(pdu_out->ssn, buffer + offset, SSN_LENGTH);
    offset += SSN_LENGTH;

    // Print SSN for debugging
    /*{
        char ssn_str[SSN_LENGTH + 1];
        memcpy(ssn_str, pdu_out->ssn, SSN_LENGTH);
        ssn_str[SSN_LENGTH] = '\0';
        printf("SSN: %s (offset: %zu)\n", ssn_str, offset);
    }*/

    // Name Length (1 byte)
    pdu_out->name_length = buffer[offset++];
    if (pdu_out->name_length > MAX_NAME_LENGTH) {
        printf("Name length is too long\n");
        return false;
    }
    //printf("Name Length: %d (offset: %zu)\n", pdu_out->name_length, offset);

    // Name (exact name_length bytes, no null terminator)
    pdu_out->name = malloc(pdu_out->name_length);
    if (!pdu_out->name) {
        perror("Memory allocation failed for name");
        return false;
    }
    memcpy(pdu_out->name, buffer + offset, pdu_out->name_length);
    offset += pdu_out->name_length;

    // For debugging, print with a length specifier
    //printf("Name: %.*s (offset: %zu)\n", pdu_out->name_length, pdu_out->name, offset);

    // Email Length (1 byte)
    pdu_out->email_length = buffer[offset++];
    if (pdu_out->email_length > MAX_EMAIL_LENGTH) {
        printf("Email length is too long\n");
        free(pdu_out->name);
        return false;
    }
    //printf("Email Length: %d (offset: %zu)\n", pdu_out->email_length, offset);

    // Email (exact email_length bytes, no null terminator)
    pdu_out->email = malloc(pdu_out->email_length);
    if (!pdu_out->email) {
        perror("Memory allocation failed for email");
        free(pdu_out->name);
        return false;
    }
    memcpy(pdu_out->email, buffer + offset, pdu_out->email_length);
    offset += pdu_out->email_length;

    //printf("Email: %.*s (offset: %zu)\n", pdu_out->email_length, pdu_out->email, offset);

    return true;
}

/*
	####REMOOOOOOOOOOOOOOOOOOOOVE###
*/
static void remove_value(Node* node, struct VAL_REMOVE_PDU* pdu) {
    uint8_t hash_value = hash_ssn(pdu->ssn);

    printf("Hash value: %d\n", hash_value);
    if (hash_value >= node->hash_range_start && hash_value <= node->hash_range_end) {
        printf("Removing value with SSN: %.12s\n", pdu->ssn);

        // Find the entry in the hash table
        Entry* entry = (Entry*)ht_remove(node->hash_table, pdu->ssn);
        if (!entry) {
            printf("Entry with SSN %.12s not found in the hash table\n", pdu->ssn);
            return;
        }

        // Free that memory
        printf("Freeing the memory\n");
        //free(entry->ssn);
        //free(entry->name);
        //free(entry->email);
        //free(entry);
        printf("Removed value with SSN: %.12s\n", pdu->ssn);
    } else {
        printf("Value with SSN: %.12s is out of range for this node.\n", pdu->ssn);
    }
}

static bool parse_val_remove_pdu(const uint8_t* buffer, struct VAL_REMOVE_PDU* pdu_out) {
    size_t offset = 0;

    printf("Parsing PDU for removal\n");
    pdu_out->type = buffer[offset++];
    printf("Type: %d (offset: %zu)\n", pdu_out->type, offset);

    memcpy(pdu_out->ssn, buffer + offset, SSN_LENGTH);
    offset += SSN_LENGTH;

    // Ensure SSN is null-terminated IMPORTANT!!
    pdu_out->ssn[SSN_LENGTH] = '\0';
    printf("SSN: %s (offset: %zu)\n", pdu_out->ssn, offset);

    return true;
}

static void lookup_value(Node* node, struct VAL_LOOKUP_PDU* pdu) {
    if (!node || !pdu || !node->hash_table) {
        fprintf(stderr, "Invalid arguments to lookup_value\n");
        return;
    }

    uint8_t hash_value = hash_ssn(pdu->ssn);
    printf("Hash value: %d\n", hash_value);

    // Check if the hash value falls within this node's range
    if (hash_value >= node->hash_range_start && hash_value <= node->hash_range_end) {
        printf("Looking up value with SSN: %.12s\n", pdu->ssn);

        // Find the entry in the hash table
        Entry* entry = ht_lookup(node->hash_table, pdu->ssn);
        if (!entry) {
            printf("Entry with SSN %.12s not found in the hash table\n", pdu->ssn);
            // Send a response indicating the entry was not found
            struct VAL_LOOKUP_RESPONSE_PDU response_pdu = {0};
            response_pdu.type = VAL_LOOKUP_RESPONSE;
            memset(response_pdu.ssn, '0', SSN_LENGTH);
            response_pdu.name_length = 0;
            response_pdu.email_length = 0;

            struct sockaddr_in client_addr;
            memset(&client_addr, 0, sizeof(client_addr));
            client_addr.sin_family = AF_INET;
            client_addr.sin_addr.s_addr = pdu->sender_address;
            client_addr.sin_port = htons(pdu->sender_port); // Convert to network byte order
            send_lookup_response(node, &response_pdu, &client_addr);
            return;
        }

        // Log the found entry
        printf("Found entry with SSN: %.12s\n", entry->ssn);
        printf("Name: %.*s\n", entry->name_length, entry->name);
        printf("Email: %.*s\n", entry->email_length, entry->email);

        // Prepare the response PDU
        struct VAL_LOOKUP_RESPONSE_PDU response_pdu = {0};
        response_pdu.type = VAL_LOOKUP_RESPONSE;
        memcpy(response_pdu.ssn, entry->ssn, SSN_LENGTH);
        response_pdu.name_length = entry->name_length;
        response_pdu.name = entry->name;
        response_pdu.email_length = entry->email_length;
        response_pdu.email = entry->email;

        // Send the response for the found entry
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = pdu->sender_address;
        client_addr.sin_port = htons(pdu->sender_port); // Convert to network byte order
        send_lookup_response(node, &response_pdu, &client_addr);
    } else {
        // Forward the lookup request to the successor
        printf("Forwarding VAL_LOOKUP to successor\n");

        size_t pdu_size = 1 + SSN_LENGTH + sizeof(uint32_t) + sizeof(uint16_t);
        uint8_t* out_buffer = malloc(pdu_size);
        if (!out_buffer) {
            perror("Memory allocation failed for PDU buffer");
            return;
        }

        size_t offset = 0;
        out_buffer[offset++] = VAL_LOOKUP;
        memcpy(out_buffer + offset, pdu->ssn, SSN_LENGTH);
        offset += SSN_LENGTH;

        *(uint32_t*)(out_buffer + offset) = pdu->sender_address;
        offset += sizeof(uint32_t);
        *(uint16_t*)(out_buffer + offset) = htons(pdu->sender_port); // Convert to network byte order
        offset += sizeof(uint16_t);

        int send_status = send(node->sockfd_b, out_buffer, pdu_size, 0);
        if (send_status == -1) {
            perror("send failure");
        } else {
            printf("VAL_LOOKUP forwarded to successor\n");
        }

        free(out_buffer);
    }
}

static void send_lookup_response(Node* node, struct VAL_LOOKUP_RESPONSE_PDU* response_pdu, struct sockaddr_in* client_addr) {
    // Calculate the total size of the PDU buffer
    size_t pdu_size = 1 + SSN_LENGTH + 1 + response_pdu->name_length + 1 + response_pdu->email_length;
    uint8_t* out_buffer = malloc(pdu_size);
    if (!out_buffer) {
        perror("Memory allocation failed for PDU buffer");
        return;
    }
    size_t offset = 0;
    out_buffer[offset++] = response_pdu->type;
    memcpy(out_buffer + offset, response_pdu->ssn, SSN_LENGTH);
    offset += SSN_LENGTH;

    out_buffer[offset++] = response_pdu->name_length;
    memcpy(out_buffer + offset, response_pdu->name, response_pdu->name_length);
    offset += response_pdu->name_length;

    out_buffer[offset++] = response_pdu->email_length;
    memcpy(out_buffer + offset, response_pdu->email, response_pdu->email_length);
    offset += response_pdu->email_length;

    printf("Sending response PDU to client at %s:%d\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
    int send_status = sendto(node->sockfd_a, out_buffer, pdu_size, 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
    if (send_status == -1) {
        perror("sendto failure");
    } else {
        printf("Response sent successfully\n");
    }

    free(out_buffer);
}


static bool parse_val_lookup_pdu(const uint8_t* buffer, struct VAL_LOOKUP_PDU* pdu_out) {
    size_t offset = 0;

    printf("Parsing PDU for lookup\n");
    pdu_out->type = buffer[offset++];
    printf("Type: %d (offset: %zu)\n", pdu_out->type, offset);

    memcpy(pdu_out->ssn, buffer + offset, SSN_LENGTH);
    offset += SSN_LENGTH;

    // Print SSN for debugging
    printf("SSN: ");
    for (size_t i = 0; i < SSN_LENGTH; i++) {
        printf("%c", pdu_out->ssn[i]);
    }
    printf(" (offset: %zu)\n", offset);

    pdu_out->sender_address = *(uint32_t*)(buffer + offset);
    offset += sizeof(uint32_t);
    pdu_out->sender_port = ntohs(*(uint16_t*)(buffer + offset)); // Convert from network byte order
    offset += sizeof(uint16_t);

    printf("Sender address: %s, Sender port: %d\n", inet_ntoa(*(struct in_addr*)&pdu_out->sender_address), pdu_out->sender_port);

    return true;
}

