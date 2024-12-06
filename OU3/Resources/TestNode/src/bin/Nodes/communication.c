
#include "communication.h"

// the state will recieve the NET_JOIN_RESPONSE from the state 7,
// it contains the information about the next node in the network.

static void split_range(Node *n, uint8_t *successor_start, uint8_t *successor_end){

    uint8_t mid_point = (n->hash_range_end - n->hash_range_start) / 2 + n->hash_range_start;
    *successor_start = mid_point + 1;
    *successor_end = n->hash_range_end;
    n->hash_range_end = mid_point;
    //printf("successor start: %d, successor end: %d\n", *successor_start, *successor_end); 

}

// that state will extract the information from the NET_JOIN_PDU and store it in the node.
// it will also create a socket for the B endpoint communication.
// as in the pdu structure states that we update the pdu max fields... 
int q12_state(void* n, void* data) {
    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;
    printf("[q12 state]\n");


    // Check if the node has no successor or predecessor
    // thus the node's public address is the maximum address
    if (node->predecessor_ip_address.s_addr == INADDR_NONE && node->successor_ip_address.s_addr == INADDR_NONE) {
        // Move to Q5 state
        net_join->max_address = node->public_ip.s_addr;
        net_join->max_port = node->port;
        net_join->max_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
        printf("I am alone in the network. Moving to state Q5...\n");
        node->state_handler = state_handlers[4];
        node->state_handler(node, net_join);
    } else if(net_join->max_address == node->public_ip.s_addr){
        // now we will check if the max address is matches with the node's public address.
        // however, be cautious wiht the byte order.
        printf("did we reach here\n");
        // as it matched thus we assumed the node is the max node in the network.
        // it measn the node has the highest hash range.
        net_join->max_address = node->public_ip.s_addr;
        net_join->max_port = node->port;
        net_join->max_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
        printf("I am the max node in the network. Moving to state Q5...\n");
        node->state_handler = state_handlers[STATE_13];
        node->state_handler(node, net_join);

    }
    else{
        // we will forward the NET_JOIN to the node's successor.
        // we will move to state 14.
        printf("Forwarding NET_JOIN to the successor...\n");
        printf("max hash span: %d\n", net_join->max_span);
        printf("nodes hash span: %d\n", node->hash_span);
        if(node->hash_span >= net_join->max_span){
            net_join->max_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
            net_join->max_address = node->public_ip.s_addr;
            net_join->max_port = node->port;
            node->state_handler = state_handlers[STATE_14];
            node->state_handler(node, net_join);
        }
        else{
            node->state_handler = state_handlers[STATE_14];
            node->state_handler(node, net_join);
        }
    }

    return 0;
}


// State function for q5
int q5_state(void* n, void* data) {
    printf("q5 state\n");

    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;

    // Set the successor address and port from the received NET_JOIN PDU
    // as the its the single node that existed in the network.
    node->successor_ip_address.s_addr = net_join->src_address;
    node->successor_port = net_join->src_port;

    printf("before slipting the has range start: %d, end: %d\n", node->hash_range_start, node->hash_range_end);
    
    uint8_t successor_start = 0, successor_end = 0;
    split_range(node, &successor_start, &successor_end);
    printf("the successor start: %d, end: %d\n", successor_start, successor_end);

    // Prepare the NET_JOIN_RESPONSE PDU, 
    // they will be each other's successor and predecessor (A<->B)
    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};
    net_join_response.type = NET_JOIN_RESPONSE;
    net_join_response.next_address = htonl(node->public_ip.s_addr);
    net_join_response.next_port = htons(node->port);
    net_join_response.range_start = successor_start;  // Just for testing purposes
    net_join_response.range_end = successor_end;

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
int q14_state(void* n, void* data){

    printf("[q14 state]\n");
    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;

    // now we will forward the NET_JOIN to the node's successor.
    int send_status = send(node->sockfd_b, net_join, sizeof(*net_join), 0);
    if (send_status == -1) {
        perror("send failure");
        return 1;
    }

    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}

// we will send the NET_CLOSE_CONNECTION to the successor.
// we will connect with the prospect afterwards
// Send join request to the prospect aka the new node.
// Transfer upper half of the hash range to the successor.
//  return to the q6 state.
int q13_state(void* n, void* data){

    printf("[q13 state]\n");
    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;

    // Prepare the NET_CLOSE_CONNECTION PDU so that the successor can close the connection with the current node.
    struct NET_CLOSE_CONNECTION_PDU net_close_connection = {0};
    net_close_connection.type = NET_CLOSE_CONNECTION;

    printf("Sending NET_CLOSE_CONNECTION to the successor...\n");
    // I think we shall close the file descriptor that is connected to the successor.
    // I mean close the socket that is connected to the successor.


    // Send the NET_CLOSE_CONNECTION PDU to the successor
    int send_status = send(node->sockfd_b, &net_close_connection, sizeof(net_close_connection), 0);
    if (send_status == -1) {
        perror("send failure");
        return 1;
    }

    // connect to the prospect aka the new node
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = net_join->src_address;
    addr.sin_port = htons(net_join->src_port);
    

    node->sockfd_b = socket(AF_INET, SOCK_STREAM, 0);
    if (node->sockfd_b == -1) {
        perror("socket failure");
        return 1;
    }

    int connect_status = connect(node->sockfd_b, (struct sockaddr*)&addr, sizeof(addr));
    if (connect_status == -1) {
        perror("connect failure");
        return 1;
    }

    printf("Connected to the new successor: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    uint8_t successor_start, successor_end;
    // now we will transfer the upper half of the hash range to the successor.
    split_range(node, &successor_start, &successor_end);

    // Prepare the NET_JOIN_RESPONSE PDU    
    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};
    net_join_response.type = NET_JOIN_RESPONSE;
    net_join_response.next_address = htonl(node->successor_ip_address.s_addr);
    net_join_response.next_port = node->successor_port;
    net_join_response.range_start = successor_start; // it will be the new node's start range.
    net_join_response.range_end =  successor_end; // same, it will be the new node's end range.


    // Send the NET_JOIN_RESPONSE PDU to the successor aka to the prospect.
    send_status = send(node->sockfd_b, &net_join_response, sizeof(net_join_response), 0);
    if (send_status == -1) {
        perror("send failure");
        return 1;
    }

    // the successor of the new node's address of the new node.
    printf("Successor of the new node net_join: %s:%d\n", inet_ntoa(node->successor_ip_address), ntohs(node->successor_port));

    // Move to the next state (q6)
    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}


int q17_state(void* n, void* data){

    // disconnect the socket
    printf("[q17 state]\n");
    Node* node = (Node*)n;
    shutdown(node->sockfd_d, SHUT_RDWR);
    close(node->sockfd_d);

    if(node->hash_range_start == 0 && node->hash_range_end == 255){
        printf("I am the only node in the network. Moving to state Q5...\n");
        node->state_handler = state_handlers[STATE_6];
        node->state_handler(node, NULL);
        return 0;
    }

    // acceot the new predecessor
    struct sockaddr_in predecessor_addr = {0};
    socklen_t addr_len = sizeof(predecessor_addr);

    printf("Accepting new predecessor...\n");

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

    printf("the successor ip address: %s\n", inet_ntoa(node->successor_ip_address));
    printf("the successor port: %d\n", node->successor_port);
    
    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}


