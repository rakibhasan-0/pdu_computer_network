
#include "communication.h"

// the state will recieve the NET_JOIN_RESPONSE from the state 7,
// it contains the information about the next node in the network.



// that state will extract the information from the NET_JOIN_PDU and store it in the node.
// it will also create a socket for the B endpoint communication.
// as in the pdu structure states that we update the pdu max fields... 
int q12_state(void* n, void* data) {
    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;
    printf("[q12 state]\n");


    net_join->max_address = node->public_ip.s_addr;
    net_join->max_port = node->port;
    net_join->max_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);


    // Check if the node has no successor or predecessor
    if (node->predecessor_ip_address.s_addr == INADDR_NONE && node->successor_ip_address.s_addr == INADDR_NONE) {
        // Move to Q5 state
        printf("I am alone in the network. Moving to state Q5...\n");
        node->state_handler = state_handlers[4];
        node->state_handler(node, net_join);
    }else{
        // for the random resaon we are moving to the q5 state.
        node->state_handler = state_handlers[4];
        node->state_handler(node, net_join);
    }

    return 0;
}


// State function for q5
int q5_state(void* n, void* data) {
    printf("q5 state\n");

    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;

    // Set the successor address and port from the received NET_JOIN PDU
    node->successor_ip_address.s_addr = net_join->src_address;
    node->successor_port = net_join->src_port;

    // Prepare the NET_JOIN_RESPONSE PDU
    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};
    net_join_response.type = NET_JOIN_RESPONSE;
    net_join_response.next_address = htonl(node->public_ip.s_addr);
    net_join_response.next_port = htons(node->port);
    net_join_response.range_start = 128;  // Just for testing purposes
    net_join_response.range_end = 255;

    node->sockfd_b = socket(AF_INET, SOCK_STREAM, 0);
    if (node->sockfd_b == -1) {
        perror("socket failure");
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = node->successor_ip_address.s_addr;
    addr.sin_port = htons(node->successor_port);

    // Connect to the successor
    int connect_status = connect(node->sockfd_b, (struct sockaddr*)&addr, sizeof(addr));
    if (connect_status == -1) {
        perror("connect failure");
        return 1;
    }

    printf("Connected to the new successor: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    int send_status = send(node->sockfd_b, &net_join_response, sizeof(net_join_response), 0);
    if (send_status == -1) {
        perror("send failure");
        return 1;
    }
    printf("The previous hash range was (%d, %d) ", node->hash_range_start, node->hash_range_end);
    node->hash_range_start = 0;  // Just for testing purposes
    node->hash_range_end = 127;  // Just for testing purposes
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
    printf("The new hash range is (%d, %d)\n", node->hash_range_start, node->hash_range_end);

    struct sockaddr_in predecessor_addr = {0};
    socklen_t addr_len = sizeof(predecessor_addr);


    // to accept something that socket must receive something from that following socket.
    // in that case from its predecessor.
    // I dont undestand what the hell it means by accepting the predecessor.
    int accept_status = accept(node->listener_socket, (struct sockaddr*)&predecessor_addr, &addr_len);
    if (accept_status == -1) {
        perror("accept failed");
        return 1;
    }

    node->predecessor_ip_address.s_addr = ntohl(predecessor_addr.sin_addr.s_addr);
    node->predecessor_port = ntohs(predecessor_addr.sin_port);
    node->sockfd_d = accept_status;

    printf("Accepted new predecessor: %s:%d\n", inet_ntoa(predecessor_addr.sin_addr),
            ntohs(predecessor_addr.sin_port));

    // Transition to Q6 after accepting predecessor
    printf("Transitioning to state Q6...\n");
    node->state_handler = state_handlers[5];
    node->state_handler(node, NULL);

    return 0;
}


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
int q9_state(void* n, void* data) {
    Node* node = (Node*)n;
    printf("[q9 state]\n");

    struct PDU pdu;
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);

    int recv_status = recvfrom(node->sockfd_a, &pdu, sizeof(pdu), 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
    if (recv_status == -1) {
        perror("recvfrom failed");
        return 1;
    }

    switch (pdu.type) {
        case VAL_INSERT:
            // Handle value insertion
            printf("Handling VAL_INSERT\n");
            // Insert value into the node's data structure
            break;
        case VAL_REMOVE:
            // Handle value removal
            printf("Handling VAL_REMOVE\n");
            // Remove value from the node's data structure
            break;
        case VAL_LOOKUP:
            // Handle value lookup
            printf("Handling VAL_LOOKUP\n");
            // Lookup value in the node's data structure
            break;
        default:
            printf("Unknown PDU type: %d\n", pdu.type);
            break;
    }

    // Transition back to Q6 state after handling Q9 operations
    node->state_handler = state_handlers[5]; // Q6 state
    node->state_handler(node, NULL);

    return 0;
}