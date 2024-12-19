#include "utils.h"

uint8_t calulate_hash_span(uint8_t start, uint8_t end){
    return (end - start ) % 256;
}


void update_hash_range(void* n, uint8_t new_range_start, uint8_t new_range_end) {

    //printf("Current hash range: [%d, %d)\n", node->hash_range_start, node->hash_range_end);
    Node* node = (Node*)n;


    // Validate the new range to ensure it's consistent with DHT logic
    if (new_range_start < 0 || new_range_end > 255 || new_range_start > new_range_end) {
        printf("Invalid hash range\n");
        return;
    }
    node->hash_range_start = new_range_start;
    node->hash_range_end = new_range_end;

    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
    //printf("Updated hash span: %d\n", node->hash_span);
}




uint8_t* constructing_insert_pdu (struct VAL_INSERT_PDU* pdu, size_t pdu_size){

    size_t offset = 0;

    uint8_t* out_buffer = malloc(pdu_size);
    if (!out_buffer) {
        perror("Memory allocation failed for PDU buffer");
        exit(EXIT_FAILURE);
    }
   
    out_buffer[offset++] = pdu->type;
    memcpy(out_buffer + offset, pdu->ssn, SSN_LENGTH);
    offset += SSN_LENGTH;

    out_buffer[offset++] = pdu->name_length;
    memcpy(out_buffer + offset, pdu->name, pdu->name_length);
    offset += pdu->name_length;

    out_buffer[offset++] = pdu->email_length;
    memcpy(out_buffer + offset, pdu->email, pdu->email_length);
    offset += pdu->email_length;

    return out_buffer;

}


void transfer_upper_half(void* node, uint8_t range_start, uint8_t range_end){

    Node* n = (Node*)node;
    printf("[transfer_upper_half]\n");

    char* entries_to_transfer[1024];
    int entries_count = 0; 

    for(int i = 0; i < MAX_SIZE ; i++){
        node_t* current_entry = n->hash_table->entries[i];
         
        while(current_entry != NULL){
            Entry* entry = (Entry*)current_entry->value;
            uint8_t hash_value = hash_ssn(entry->ssn);
            printf("Hash value: %d\n", hash_value);

            if(hash_value >= range_start && hash_value <= range_end){
                entries_to_transfer[entries_count++] = entry->ssn;
            }
            current_entry = current_entry->next;
        }

    }


    printf("Transferring %d entries to the successor\n", entries_count);

    for(int i = 0; i < entries_count; i++){

        char* ssn = entries_to_transfer[i];
        Entry* entry = (Entry*)ht_lookup(n->hash_table, ssn);
        // use length to print
        //printf("The values value with SSN: %.*s\n", SSN_LENGTH, entry->ssn);
        //printf("Name: %.*s\n", (int)entry->name_length, entry->name);
        //printf("Email: %.*s\n", (int)entry->email_length, entry->email);
        
        struct VAL_INSERT_PDU val_insert = {0};
        val_insert.type = VAL_INSERT;
        memcpy(val_insert.ssn, entry->ssn, SSN_LENGTH);
        val_insert.name_length = entry->name_length;
        val_insert.name = entry->name;
        val_insert.email_length = entry->email_length;
        val_insert.email = entry->email;

        size_t pdu_size = 1 + SSN_LENGTH + 1 + entry->name_length + 1 + entry->email_length;

        uint8_t* out_buffer = constructing_insert_pdu(&val_insert, pdu_size);
        int send_status = send(n->sockfd_b, out_buffer,pdu_size, 0);
        if (send_status == -1) {
            perror("send failure");
        } else {
            printf("VAL_INSERT forwarded to successor\n");
        }

        ht_remove(n->hash_table, ssn);

        free(out_buffer);
        free(entry->ssn);
        free(entry->name);
        free(entry->email);
        free(entry);
        sleep(1);

    }   

}

// that function will be called from the state 18.
// we will transfer all the entries to the successor or the predecessor.
void transfer_all_entries(void* n, bool to_successor){

    Node* noed = (Node*)n;
    printf("[transfer_all_entries]\n");

    // get the existing entries from the hash table.
    // I will think about it, whether we need to use dynamic memory allocation or not for the entries.
    char* entries_to_transfer[1024];
    int entries_count = 0;


    // we are iterating over the hash table to get all the entries.
    // and save them in the entries_to_transfer array based on the ssns.
    for(int i = 0; i < MAX_SIZE; i++){
        node_t* current_entry = noed->hash_table->entries[i];
        while(current_entry != NULL){
            Entry* entry = (Entry*)current_entry->value;
            entries_to_transfer[entries_count++] = entry->ssn;
            current_entry = current_entry->next; // since it is chained hash table.
        }
    }

    printf("Transferring %d entries to the successor\n", entries_count);

    for(int i = 0; i < entries_count; i++){
        char* ssn = entries_to_transfer[i];
        Entry* entry = (Entry*)ht_lookup(noed->hash_table, ssn);
        // use length to print
        printf("The values value with SSN: %.*s\n", SSN_LENGTH, entry->ssn);
        printf("Name: %.*s\n", (int)entry->name_length, entry->name);
        printf("Email: %.*s\n", (int)entry->email_length, entry->email);
        
        struct VAL_INSERT_PDU val_insert = {0};
        val_insert.type = VAL_INSERT;
        memcpy(val_insert.ssn, entry->ssn, SSN_LENGTH);
        val_insert.name_length = entry->name_length;
        val_insert.name = entry->name;
        val_insert.email_length = entry->email_length;
        val_insert.email = entry->email;

        size_t pdu_size = 1 + SSN_LENGTH + 1 + entry->name_length + 1 + entry->email_length;

        uint8_t* out_buffer = constructing_insert_pdu(&val_insert, pdu_size);
        int send_status;
        if(to_successor){
            send_status = send(noed->sockfd_b, out_buffer,pdu_size, 0);
        }
        else {
            send_status = send(noed->sockfd_d, out_buffer,pdu_size, 0);
        }
        if (send_status == -1) {
            perror("send failure");
        } else {
            printf("VAL_INSERT forwarded to successor\n");
        }

        ht_remove(noed->hash_table, ssn);

        free(out_buffer);
        free(entry->ssn);
        free(entry->name);
        free(entry->email);
        free(entry);
        sleep(1);

    }


}
