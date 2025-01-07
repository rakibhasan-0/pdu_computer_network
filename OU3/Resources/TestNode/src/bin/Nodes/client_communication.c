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
static void lookup_value(Node* node, struct VAL_LOOKUP_PDU* pdu);
static void remove_value(Node* node, struct VAL_REMOVE_PDU* pdu);
static bool parse_val_remove_pdu(const uint8_t* buffer, struct VAL_REMOVE_PDU* pdu_out);
static bool parse_val_lookup_pdu(const uint8_t* buffer, struct VAL_LOOKUP_PDU* pdu_out);
static void send_lookup_response(Node* node, struct VAL_LOOKUP_RESPONSE_PDU* response_pdu, struct sockaddr_in* client_addr);

int q9_state(void* n, void* data) {
    Node* node = (Node*)n;
    //printf("[Q9 state]\n");sds

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
			if (parse_val_remove_pdu(pdu->buffer, &val_remove_pdu)) {
				printf("SSN: %s\n", val_remove_pdu.ssn);
				remove_value(node, &val_remove_pdu);
			}
            break;
        case VAL_LOOKUP: 
            printf("Handling VAL_LOOKUP\n");
			struct VAL_LOOKUP_PDU val_lookup_pdu;
			printf("Current sockfd_b lookup: %d\n", node->sockfd_b);
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
    if (!node || !pdu || !node->hash_table) {
        fprintf(stderr, "Invalid arguments to remove_value\n");
        return;
    }

    uint8_t hash_value = hash_ssn((char *)pdu->ssn);
    printf("Hash value: %d\n", hash_value);

    // Check if the hash value falls within this node's range
    if (hash_value >= node->hash_range_start && hash_value <= node->hash_range_end) {
        printf("Removing value with SSN: %.12s\n", pdu->ssn);

        // Remove the entry from the hash table
        ht_remove(node->hash_table, (char *)pdu->ssn);
        printf("Value with SSN %.12s removed successfully\n", pdu->ssn);
    } else {
        // Forward the remove request to the successor
        printf("Forwarding VAL_REMOVE to successor\n");

        size_t pdu_size = 1 + SSN_LENGTH;
        uint8_t* out_buffer = malloc(pdu_size);
        if (!out_buffer) {
            perror("Memory allocation failed");
            return;
        }

        size_t offset = 0;
        out_buffer[offset++] = VAL_REMOVE;
        memcpy(out_buffer + offset, pdu->ssn, SSN_LENGTH);
        offset += SSN_LENGTH;

        int send_status = send(node->sockfd_b, out_buffer, pdu_size, 0);
        if (send_status == -1) {
            perror("send failure");
        } else {
            printf("VAL_REMOVE forwarded to successor\n");
        }

        free(out_buffer);
    }
}

static bool parse_val_remove_pdu(const uint8_t* buffer, struct VAL_REMOVE_PDU* pdu_out) {
    if (!buffer || !pdu_out) {
        return false;
    }
    size_t offset = 0;
    pdu_out->type = buffer[offset++];  // Type (1 byte)
    memcpy(pdu_out->ssn, buffer + offset, SSN_LENGTH); // SSN (12 bytes)
    offset += SSN_LENGTH;

    return true;
}

/*
	####LOOOOKUP###
*/
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
			//Skill issue write correctly
        }

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

		// Write the sender address (32-bit value) into the buffer
		memcpy(out_buffer + offset, &pdu->sender_address, sizeof(uint32_t));
		offset += sizeof(uint32_t); // Move the offset forward by the size of a 32-bit value

		// Write the sender port (16-bit value) into the buffer, converting to network byte order
		uint16_t network_port = htons(pdu->sender_port);
		memcpy(out_buffer + offset, &network_port, sizeof(uint16_t));
		offset += sizeof(uint16_t); // Move the offset forward by the size of a 16-bit value

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

	// Copy the sender address (32-bit value) from the buffer
	memcpy(&pdu_out->sender_address, buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);

	// Copy the sender port (16-bit value) from the buffer and convert from network byte order
	uint16_t network_port;
	memcpy(&network_port, buffer + offset, sizeof(uint16_t));
	pdu_out->sender_port = ntohs(network_port);
	offset += sizeof(uint16_t);

    return true;
}
/*
htons (Host to Network Short)
ntohs (Network to Host Short)
sockfd_a is the UDP socket
*/
/*There are many implicit and explicit rules of the distributed hash table, some
of the most important ones are listed below.

Only one insertion of a node is performed at a time.
SSN is represented as 12-bytes, YYYYMMDDXXXX, no null termination.
Name and email fields are at most 255 characters, no null termination.
Invalid PDUs can be dropped without response.
All port fields are transmitted in network byte order, also known as big-endian.

It is crucial that you handle this correctly, as the communication will not function
if there is cross-network varying byte order.
Check the htons and ntohs functions in C

Note: It is assumed that no PDU sent over UDP can get corrupted or get dropped.*/