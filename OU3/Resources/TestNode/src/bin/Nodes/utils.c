#include "utils.h"

uint8_t calulate_hash_span(uint8_t start, uint8_t end){
    return (end - start ) % 256;
}


void update_hash_range(void* n, uint8_t new_range_start, uint8_t new_range_end) {

    //printf("Current hash range: [%d, %d)\n", node->hash_range_start, node->hash_range_end);
    Node* node = (Node*)n;

    printf("[update_hash_range]\n");
    printf("Current range: (%d, %d)\n", node->hash_range_start, node->hash_range_end);
    printf("Updating range to: (%d, %d)\n", new_range_start, new_range_end);

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
