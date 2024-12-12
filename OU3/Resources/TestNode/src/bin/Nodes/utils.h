#ifndef UTILS_H
#define UTILS_H

#include "node.h"

uint8_t calulate_hash_span(uint8_t start, uint8_t end);
void update_hash_range(void* n, uint8_t new_range_start, uint8_t new_range_end);

#endif