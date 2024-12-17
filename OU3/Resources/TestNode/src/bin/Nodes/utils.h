#ifndef UTILS_H
#define UTILS_H

#include "node.h"
#include "communication.h"
#include "join_network.h"
#include "signal_handler.h"
#include "utils.h"
#include "initialization.h"
#include "hashtable.h"

struct Node;

uint8_t calulate_hash_span(uint8_t start, uint8_t end);
void update_hash_range(void* n, uint8_t new_range_start, uint8_t new_range_end);
void transfer_upper_half(void* node, uint8_t range_start, uint8_t range_end);
uint8_t* constructing_insert_pdu(struct VAL_INSERT_PDU* pdu, size_t size);

#endif