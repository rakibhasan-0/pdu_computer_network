#include "utils.h"

uint8_t calulate_hash_span(uint8_t start, uint8_t end){
    printf("start:--from calc hash_span %d\n", start);
    printf("end:--from clac hash_span %d\n", end);

    return (end - start + 1) % 256;
}