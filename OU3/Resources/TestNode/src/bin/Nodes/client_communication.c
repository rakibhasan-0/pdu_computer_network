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

int q9_state(void* n, void* data) {
    Node* node = (Node*)n;
    printf("[Q9 state]\n");

    char* buffer = (char*)data;
    uint8_t pdu_type = buffer[0]; // Assuming the first byte indicates the PDU type
    printf("PDU type: %d\n", pdu_type);

    switch (pdu_type) {

        case VAL_INSERT:
            printf("Handling VAL_INSERT\n");
            struct VAL_INSERT_PDU pdu = {0};

            // Parse the buffer into the PDU structure of the VAL_INSERT_PDU type
            if(parse_val_insert_pdu(buffer, &pdu)){
                printf("SSN: %s\n", pdu.ssn);
                printf("Name: %s\n", pdu.name);
                printf("Email: %s\n", pdu.email);
                insertion_of_value(node, &pdu);
                // Free allocated memory
                free(pdu.name);
                free(pdu.email);
            }

            break;

        case VAL_REMOVE:
            printf("Handling VAL_REMOVE\n");
            struct VAL_REMOVE_PDU* val_remove_pdu = (struct VAL_REMOVE_PDU*)buffer;
            printf("SSN: %s\n", val_remove_pdu->ssn);
            //handle_val_remove(node, val_remove_pdu);
            break;
        case VAL_LOOKUP:
            printf("Handling VAL_LOOKUP\n");
            struct VAL_LOOKUP_PDU* val_lookup_pdu = (struct VAL_LOOKUP_PDU*)buffer;
            printf("SSN: %s\n", val_lookup_pdu->ssn);
            //handle_val_lookup(node, val_lookup_pdu);
            break;
        default:
            printf("Unknown PDU type: %d\n", pdu_type);
            break;
    }

    // Transition back to Q6 state after processing the PDU
    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}

/*
// This function handles the insertion of a value into a node. 
// It takes a pointer to a Node structure and a pointer to a VAL_INSERT_PDU structure as parameters.
void handle_val_insert(Node* node, struct VAL_INSERT_PDU* pdu) {
	printf("Inserting value with SSN: %s\n", pdu->ssn);
    if (in_my_range(node, pdu->ssn)) {
        // Create a new Entry
        struct Entry* new_entry = (struct Entry*)malloc(sizeof(struct Entry));
        if (new_entry == NULL) {
            printf("Memory allocation failed for new entry.\n");
            return;
        }

        // Copy the SSN, name, and email
        memcpy(new_entry->ssn, pdu->ssn, SSN_LENGTH);
        strncpy(new_entry->name, (char*)pdu->name, pdu->name_length);
        new_entry->name[pdu->name_length] = '\0'; // Null-terminate the string
        strncpy(new_entry->email, (char*)pdu->email, pdu->email_length);
        new_entry->email[pdu->email_length] = '\0'; // Null-terminate the string

        // Insert the new entry at the head of the linked list
        new_entry->next = node->entries_head;
        node->entries_head = new_entry;

        printf("Inserting value with SSN: %s\n", pdu->ssn);
    } else {
        printf("Value with SSN: %s is out of range for this node.\n", pdu->ssn);
    }
}

// Tried copying the val_remove from node.rs
void handle_val_remove(Node* node, struct VAL_REMOVE_PDU* pdu) {
    if (in_my_range(node, pdu->ssn)) {
        printf("Removing value with SSN: %s\n", pdu->ssn);
        struct Entry** current = &node->entries_head;
        while (*current != NULL) {
            if (strncmp((*current)->ssn, pdu->ssn, SSN_LENGTH) == 0) {
                struct Entry* to_delete = *current;
                *current = (*current)->next;
                free(to_delete);
                printf("Removed value with SSN: %s\n", pdu->ssn);
                return;
            }
            current = &(*current)->next;
        }
        printf("Value with SSN: %s not found\n", pdu->ssn);
    } else {
        printf("Forwarding VAL_REMOVE to successor\n");
		// Not done here
    }
}

// Tried copying the val_lookup from node.rs
void handle_val_lookup(Node* node, struct VAL_LOOKUP_PDU* pdu) {
    if (in_my_range(node, pdu->ssn)) {
        printf("Looking up value with SSN: %s\n", pdu->ssn);
        struct Entry* current = node->entries_head;
        while (current != NULL) {
            if (strncmp(current->ssn, pdu->ssn, SSN_LENGTH) == 0) {
                printf("Found value with SSN: %s\n", pdu->ssn);
                // Send response back to the requester
                struct VAL_LOOKUP_RESPONSE_PDU response = {0};
                response.type = VAL_LOOKUP_RESPONSE;
                strncpy(response.ssn, current->ssn, SSN_LENGTH);
                strncpy(response.name, current->name, sizeof(response.name));
                strncpy(response.email, current->email, sizeof(response.email));
				// Need to add more here
                return;
            }
            current = current->next;
        }
        printf("Value with SSN: %s not found\n", pdu->ssn);
    } else {
        printf("Forwarding VAL_LOOKUP to successor\n");
		// Not done here
    }
}

*/

//this fucntion will free the entry.
// it is being passed to the ht_create function.
// it is new version for the insertaion of the node
static void insertion_of_value(Node* node, struct VAL_INSERT_PDU* pdu) {
    uint8_t hash_value = hash_ssn(pdu->ssn);

    if (hash_value >= node->hash_range_start && hash_value <= node->hash_range_end) {
        Entry* entry = malloc(sizeof(Entry));
        if (!entry) {
            perror("malloc failed for Entry");
            return;
        }

        entry->ssn = malloc(SSN_LENGTH);
        if (!entry->ssn) {
            perror("malloc failed for ssn");
            free(entry);
            return;
        }
        memcpy(entry->ssn, pdu->ssn, SSN_LENGTH);

        entry->name = malloc(pdu->name_length);
        if (!entry->name) {
            perror("malloc failed for name");
            free(entry->ssn);
            free(entry);
            return;
        }
        memcpy(entry->name, pdu->name, pdu->name_length);
        entry->name_length = pdu->name_length;

        entry->email = malloc(pdu->email_length);
        if (!entry->email) {
            perror("malloc failed for email");
            free(entry->name);
            free(entry->ssn);
            free(entry);
            return;
        }
        memcpy(entry->email, pdu->email, pdu->email_length);
        entry->email_length = pdu->email_length;

        // Use %.*s for printing without null terminators
        printf("Inserting value with SSN: %.*s\n", SSN_LENGTH, entry->ssn);
        printf("Name: %.*s\n", (int)entry->name_length, entry->name);
        printf("Email: %.*s\n", (int)entry->email_length, entry->email);

        node->hash_table = ht_insert(node->hash_table, entry->ssn, entry);
        printf("Value inserted successfully\n");
    } else {
        // Forward to successor
        size_t pdu_size = 1 + SSN_LENGTH + 1 + pdu->name_length + 1 + pdu->email_length;
        uint8_t* out_buffer = constructing_insert_pdu(pdu, pdu_size);
        int send_status = send(node->sockfd_b, out_buffer, pdu_size, 0);
        if (send_status == -1) {
            perror("send failure");
        } else {
            printf("VAL_INSERT forwarded to successor\n");
        }
        free(out_buffer);
    }
}

// the purpose of this function is to parse the val_insert pdu.
static bool parse_val_insert_pdu(const uint8_t* buffer, struct VAL_INSERT_PDU* pdu_out) {
    size_t offset = 0;

    printf("Parsing PDU...\n");

    pdu_out->type = buffer[offset++];
    memcpy(pdu_out->ssn, buffer + offset, SSN_LENGTH);
    offset += SSN_LENGTH;

    pdu_out->name_length = buffer[offset++];
    pdu_out->name = malloc(pdu_out->name_length);
    if (!pdu_out->name) {
        perror("malloc failed for name");
        return false;
    }
    memcpy(pdu_out->name, buffer + offset, pdu_out->name_length);
    offset += pdu_out->name_length;

    pdu_out->email_length = buffer[offset++];
    pdu_out->email = malloc(pdu_out->email_length);
    if (!pdu_out->email) {
        perror("malloc failed for email");
        free(pdu_out->name);
        return false;
    }
    memcpy(pdu_out->email, buffer + offset, pdu_out->email_length);
    offset += pdu_out->email_length;

    // Print using length specifiers
    printf("Type: %d\n", pdu_out->type);
    printf("SSN: %.*s\n", SSN_LENGTH, pdu_out->ssn); // or treat as raw data if SSN is not guaranteed ASCII
    printf("Name: %.*s\n", pdu_out->name_length, pdu_out->name);
    printf("Email: %.*s\n", pdu_out->email_length, pdu_out->email);

    return true;
}


// I think this one works?
/*bool in_my_range(Node* node, const char* ssn) {
    uint8_t hash_value = hash_ssn(ssn);

    // Check if the hash value is within the node's range
    if (node->hash_range_start <= node->hash_range_end) {
        return hash_value >= node->hash_range_start && hash_value <= node->hash_range_end;
    } else {
        return hash_value >= node->hash_range_start || hash_value <= node->hash_range_end;
    }
}*/

/*        
		fn in_my_range(&mut self, ssn: &String) -> bool {
			let (min, max) = self.hash_range;
			let ssn_hash = Entry::hash_ssn(&ssn);
			min <= ssn_hash && ssn_hash <= max
		}

        fn handle_val_insert(&mut self, pdu: ValInsertPdu) {
            if self.in_my_range(&pdu.ssn) {
                let e = Entry::new(pdu.ssn, pdu.name, pdu.email);
                println!("    Inserting ssn {:?}", e);
                self.values.push(e);
            } else {
                if let Some(socket) = &mut self.successor {
                    self.successor_wrapper.send(socket, pdu.into());
                    println!("    Forwarding val_insert to successor");
                } else {
                    panic!("Successor is not set, impossible!");
                }
            }
        }

        fn handle_val_lookup(&mut self, pdu: ValLookupPdu) {
            if self.in_my_range(&pdu.ssn) {
                let mut found = false;
                for entry in &self.values {
                    if entry.ssn == pdu.ssn {
                        let pdu_response = ValLookupResponsePdu::new(
                            entry.ssn.clone(),
                            entry.name.clone(),
                            entry.email.clone(),
                        );
                        println!("    Value found (ssn: {}).", entry.ssn);
                        let ip = Ipv4Addr::from(pdu.sender_address);
                        let addr = SocketAddr::new(IpAddr::V4(ip), pdu.sender_port.into());
                        self.udp_wrapper
                            .send(&mut self.udp_socket, pdu_response.into(), addr);
                        found = true;
                        break;
                    }
                }

                if !found {
                    println!("    Value does not exist, responding with empty pdu");
                    let pdu_response = ValLookupResponsePdu::new(
                        "000000000000".into(),
                        String::new(),
                        String::new(),
                    );
                    let ip = Ipv4Addr::from(pdu.sender_address);
                    let addr = SocketAddr::new(IpAddr::V4(ip), pdu.sender_port.into());
                    self.udp_wrapper
                        .send(&mut self.udp_socket, pdu_response.into(), addr);
                }
            } else {
                if let Some(socket) = &mut self.successor {
                    self.successor_wrapper.send(socket, pdu.into());
                    println!("    Forwarding val_lookup to successor");
                } else {
                    panic!("Successor is not set, impossible!");
                }
            }
        }

        fn handle_val_remove(&mut self, pdu: ValRemovePdu) {
            if self.in_my_range(&pdu.ssn) {
                println!("    Removing ssn {}", pdu.ssn);
                self.values.retain(|x| x.ssn != pdu.ssn);
            } else {
                if let Some(socket) = &mut self.successor {
                    self.successor_wrapper.send(socket, pdu.into());
                    println!("    Forwarding val_remove to successor");
                } else {
                    panic!("Successor is not set, impossible!");
                }
            }
        }

*/

