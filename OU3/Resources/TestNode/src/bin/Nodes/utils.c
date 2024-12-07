#include "utils.h"

uint8_t calulate_hash_span(uint8_t start, uint8_t end){
    return (end - start ) % 256;
}