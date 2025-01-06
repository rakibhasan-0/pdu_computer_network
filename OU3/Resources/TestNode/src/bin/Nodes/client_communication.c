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
static void parse_val_lookup_pdu(const uint8_t* buffer, struct VAL_LOOKUP_PDU* pdu_out);
void send_val_lookup_response(const struct Entry* entry, const struct VAL_LOOKUP_PDU* pdu, Node* node);
static int sererialize_val_lookup_response(const struct Entry* entry, uint8_t* buffer);

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
            printf("PDU buffer size: %zu\n", sizeof(pdu->buffer));
            printf("NET_JOIN PDU raw buffer (size=%zd):\n", pdu->size);
            
            for (size_t i = 0; i < 19; i++) {
                printf("%02X ", (unsigned char)pdu->buffer[i]);
            }
            printf("\n");

            struct VAL_LOOKUP_PDU val_lookup_pdu = {0};
            parse_val_lookup_pdu(pdu->buffer, &val_lookup_pdu);
            lookup_value(node, &val_lookup_pdu);
        
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
        //printf("Inserting value with SSN: %.12s\n", pdu->ssn);

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
        printf("Value inserted successfully\n");
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
        printf("Forwarding VAL_INSERT to successor\n");

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

    // getting the hash value of the ssn.
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
            Entry dummy_entry = {0};
            dummy_entry.ssn = "000000000000";
            dummy_entry.name = "0";
            dummy_entry.email = "0";
            dummy_entry.name_length = 1;
            dummy_entry.email_length = 1;

            send_val_lookup_response(&dummy_entry, pdu, node);
        }

        // Log the found entry
        printf("Found entry with SSN: %.12s\n", entry->ssn);
        printf("Name: %.*s\n", entry->name_length, entry->name);
        printf("Email: %.*s\n", entry->email_length, entry->email);

        // Send the response for the found entry
        send_val_lookup_response(entry, pdu, node);

    } else {
        // Forward the lookup request to the successor
        printf("Forwarding VAL_LOOKUP to successor\n");

        size_t pdu_size = 1 + SSN_LENGTH + 4 + 2; // type + SSN + sender_address + sender_port
        uint8_t* out_buffer = malloc(pdu_size);
        if (!out_buffer) {
            perror("Memory allocation failed for PDU buffer");
            return;
        }

        size_t offset = 0;
        out_buffer[offset++] = VAL_LOOKUP;
        memcpy(out_buffer + offset, pdu->ssn, SSN_LENGTH);
        offset += SSN_LENGTH;

        // Copy sender address and port
        memcpy(out_buffer + offset, &pdu->sender_address, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        // I guess we need to make the port as the network order.
        uint16_t port = htons(pdu->sender_port);
        memcpy(out_buffer + offset, &port, sizeof(uint16_t));

        offset += sizeof(uint16_t);

        int send_status = send(node->sockfd_b, out_buffer, pdu_size, 0);
        if(send_status == -1){
            perror("send failure");
            return;
        }

        free(out_buffer);


    }
}

static void parse_val_lookup_pdu(const uint8_t* buffer, struct VAL_LOOKUP_PDU* pdu_out){

    size_t offset = 0;

    // 1) Type (1 byte)
    pdu_out->type = buffer[offset++];

    // 2) SSN (12 bytes)
    memcpy(pdu_out->ssn, buffer + offset, SSN_LENGTH);
    offset += SSN_LENGTH;

    // 3) sender_address (4 bytes in network order)
    memcpy(&pdu_out->sender_address, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // 4) sender_port (2 bytes in network order)
    memcpy(&pdu_out->sender_port, buffer + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    // Convert from network to host byte order for printing
    pdu_out->sender_address = ntohl(pdu_out->sender_address);
    pdu_out->sender_port    = ntohs(pdu_out->sender_port);

    // Now print them properly
    struct in_addr ip_addr;
    ip_addr.s_addr = htonl(pdu_out->sender_address); // inet_ntoa expects network order
    //printf("Client address : %s\n", inet_ntoa(ip_addr));
    //printf("Client port    : %u\n", pdu_out->sender_port);
}


void send_val_lookup_response(const struct Entry* entry, const struct VAL_LOOKUP_PDU* pdu, Node* node) {

    // serialize the entry into the buffer.
    ssize_t lookup_response_size = 1 + SSN_LENGTH + 1 + entry->name_length + 1 + entry->email_length;
    uint8_t* lookup_response_buffer = malloc(lookup_response_size);

    if (!lookup_response_buffer) {
        perror("Memory allocation failed for VAL_LOOKUP response buffer");
        return;
    }
    memset(lookup_response_buffer, 0, lookup_response_size);
    printf("the size of the lookup response buffer is %zu\n", lookup_response_size);


    uint32_t temp_sender_address = pdu->sender_address;


    struct sockaddr_in client_addr_test;
    client_addr_test.sin_family = AF_INET;
    client_addr_test.sin_port = pdu->sender_port;
    client_addr_test.sin_addr.s_addr = pdu->sender_address;

    printf("the address_temp of the client is %s\n", inet_ntoa(client_addr_test.sin_addr)); // network order
    printf("the port of the client is %d\n", client_addr_test.sin_port); // network order


    int bytes_to_send = sererialize_val_lookup_response(entry, lookup_response_buffer);
    printf("the bytes to send is %d\n", bytes_to_send);
    printf("the true respons size is %zu\n", 1 + SSN_LENGTH + 1 + entry->name_length + 1 + entry->email_length);


    // now we will send the response to the client.
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(pdu->sender_port);
    client_addr.sin_addr.s_addr = htonl(pdu->sender_address);

    printf("the address of the client is %s\n", inet_ntoa(client_addr.sin_addr));
    printf("the port of the client is %d\n", client_addr.sin_port);

    if(node->sockfd_a < 0){
        perror("socket failure");
        return;
    }

    int send_status = sendto(node->sockfd_a, lookup_response_buffer, bytes_to_send, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
    if (send_status == -1) {
        perror("Failed to send VAL_LOOKUP response");
    } else {
        printf("Lookup response sent for SSN: %.12s\n", pdu->ssn);
    }

    free(lookup_response_buffer);

}


static int sererialize_val_lookup_response(const struct Entry* entry, uint8_t* buffer){
    size_t offset = 0;

    buffer[offset++] = VAL_LOOKUP_RESPONSE;
    memcpy(buffer + offset, entry->ssn, SSN_LENGTH);
    offset += SSN_LENGTH;

    buffer[offset++] = entry->name_length;
    memcpy(buffer + offset, entry->name, entry->name_length);
    offset += entry->name_length;

    buffer[offset++] = entry->email_length;
    memcpy(buffer + offset, entry->email, entry->email_length);
    offset += entry->email_length;

    printf("the offset is %zu\n", offset);
    return offset;
}