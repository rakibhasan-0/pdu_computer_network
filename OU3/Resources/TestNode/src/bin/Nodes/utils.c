#include "utils.h"

uint8_t calulate_hash_span(uint8_t start, uint8_t end){
    return (end - start ) % 256;
}


void update_hash_range(void* n, uint8_t new_range_start, uint8_t new_range_end) {

    //printf("Current hash range: [%d, %d)\n", node->hash_range_start, node->hash_range_end);
    Node* node = (Node*)n;

    // here we are updating if the start hash range is less than the current hash range.
    if(node->hash_range_start > new_range_start){
        node->hash_range_start = new_range_start;
    }

    // here we are updating if the end hash range is greater than the current hash range.
    if(node->hash_range_end < new_range_end){
        node->hash_range_end = new_range_end;
    }
    // Recalculate the hash span
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
    //printf("Updated hash span: %d\n", node->hash_span);
}
