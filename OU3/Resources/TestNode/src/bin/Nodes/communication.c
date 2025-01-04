
#include "communication.h"


size_t serialize_net_join_response(const struct NET_JOIN_RESPONSE_PDU* pdu, char* buffer);
size_t serialize_net_close_connection(const struct NET_CLOSE_CONNECTION_PDU* pdu, char* buffer);
static int serialize_net_join(struct NET_JOIN_PDU* pdu, char* buffer);




static int serialize_net_join(struct NET_JOIN_PDU* pdu, char* buffer){
    size_t offset = 0;
    memcpy(buffer + offset, &pdu->type, sizeof(pdu->type));
    offset += sizeof(pdu->type);
    memcpy(buffer + offset, &pdu->src_address, sizeof(pdu->src_address));
    offset += sizeof(pdu->src_address);
    memcpy(buffer + offset, &pdu->src_port, sizeof(pdu->src_port));
    offset += sizeof(pdu->src_port);
    memcpy(buffer + offset, &pdu->max_span, sizeof(pdu->max_span));
    offset += sizeof(pdu->max_span);
    memcpy(buffer + offset, &pdu->max_address, sizeof(pdu->max_address));
    offset += sizeof(pdu->max_address);
    memcpy(buffer + offset, &pdu->max_port, sizeof(pdu->max_port));
    offset += sizeof(pdu->max_port);
    return offset;
}

// that function will calculate the hash span of the node and its successor.
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
// the state will recieve the NET_JOIN_RESPONSE from the state 7 or 6...
// it contains the information about the next node in the network.
int q12_state(void* n, void* data) {
    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;
    printf("[q12 state]\n");

   
    if (node->predecessor_ip_address.s_addr == INADDR_NONE && node->successor_ip_address.s_addr == INADDR_NONE) {
        // Move to Q5 state
        printf("I am alone in the network. Moving to state Q5...\n");

        node->state_handler = state_handlers[STATE_5];
        node->state_handler(node, net_join);
        
    } else if(ntohl(net_join->max_address) == ntohl(node->public_ip.s_addr) && net_join->max_port == node->port){
        // now we will check if the max address is matches with the node's public address.
        // however, be cautious wiht the byte order.
        // as it matched thus we assumed the node is the max node in the network.
        // it measn the node has the highest hash range.
        net_join->max_address = node->public_ip.s_addr;
        net_join->max_port = node->port;
        net_join->max_span = node->hash_span;

        printf("I am the max node in the network. Moving to state Q13...\n");
        node->state_handler = state_handlers[STATE_13];
        node->state_handler(node, net_join);

    }
    else{
        // we will forward the NET_JOIN to the node's successor.
        // we will move to state 14.
       // printf("Forwarding NET_JOIN to the successor...\n");

        // if the hash span of the node is greater than the max span of the network.
        // then the node will be the max node's information in the net join message.
        if(node->hash_span > net_join->max_span){
            net_join->max_span = node->hash_span;
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
    //free(data);
    return 0;
}


// State function for q5
int q5_state(void* n, void* data) {
    printf("q5 state\n");

    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;

    // Set the successor address and port from the received NET_JOIN PDU
    // as the its the single node that existed in the network.
    node->successor_ip_address.s_addr = net_join->src_address; // now we are converting the address to the network order.
    node->successor_port = net_join->src_port;


    //printf("before slipting the has range start: %d, end: %d\n", node->hash_range_start, node->hash_range_end);
    
    uint8_t successor_start = 0, successor_end = 0;
    split_range(node, &successor_start, &successor_end);
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
    printf("The node's updated hash range is: (%d, %d)\n", node->hash_range_start, node->hash_range_end);
    printf("The node's hash span: %d\n", node->hash_span);
    printf("Successor: %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);
    printf("The node's successor's hash range start: %d, end: %d\n", successor_start, successor_end);

    // Prepare the NET_JOIN_RESPONSE PDU, 
    // they will be each other's successor and predecessor (A<->B)
    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};
    net_join_response.type = NET_JOIN_RESPONSE;
    net_join_response.next_address = node->public_ip.s_addr;
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
    addr.sin_addr.s_addr = node->successor_ip_address.s_addr; // we have already stored in the network order.
    addr.sin_port = htons(node->successor_port);

    // Connect to the successor
    int connect_status = connect(node->sockfd_b, (struct sockaddr*)&addr, sizeof(addr));
    if (connect_status == -1) {
        perror("connect failure");
        return 1;
    }

    //printf("Connected to the new successor: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    char net_join_response_buffer[sizeof(net_join_response)];
    size_t bytes_to_send_net_join = serialize_net_join_response(&net_join_response, net_join_response_buffer);

    int send_status = send(node->sockfd_b, net_join_response_buffer, bytes_to_send_net_join, 0);
    if (send_status == -1) {
        perror("send failure");
        return 1;
    }

    
    // Transfer the upper half of the hash range to the successor
    transfer_upper_half(node, successor_start, successor_end);

    //node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
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

    node->predecessor_ip_address.s_addr = predecessor_addr.sin_addr.s_addr; // storing the predecessor's address in network order.
    node->predecessor_port = ntohs(predecessor_addr.sin_port); // converting the port to the host order.
    node->sockfd_d = accept_status;

    printf("Accepted new predecessor: %s:%d\n", inet_ntoa(node->predecessor_ip_address),
           node->predecessor_port);

    // Transition to Q6 after accepting predecessor
    //printf("Transitioning to state Q6...\n");
    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}

size_t serialize_net_close_connection(const struct NET_CLOSE_CONNECTION_PDU* pdu, char* buffer){
    size_t offset = 0;
    memcpy(buffer + offset, &pdu->type, sizeof(pdu->type));
    offset += sizeof(pdu->type);
    return offset;
    
}

// we will forward the NET_JOIN to the node's successor.
int q14_state(void* n, void* data){

    printf("[q14 state]\n");
    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;
    printf("Forwarding NET_JOIN to the successor...\n");
    printf("Successor: %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);

    // we have to check the byte order of the address and port.
    // we have to convert the address and port the network order.
    net_join->src_address = net_join->src_address;
    net_join->src_port = htons(net_join->src_port);
    net_join->max_address = net_join->max_address;
    net_join->max_port = htons(net_join->max_port);
    net_join->max_span = net_join->max_span;

    // now we will forward the NET_JOIN to the node's successor.
    char net_join_buffer[sizeof(struct NET_JOIN_PDU)];
    size_t bytes_to_send_net_join = serialize_net_join(net_join, net_join_buffer);
    printf("the size of the net join structure is %d\n", sizeof(struct NET_JOIN_PDU));
    printf("the size of the net join buffer is %d\n", bytes_to_send_net_join);

    int send_status = send(node->sockfd_b, net_join_buffer, bytes_to_send_net_join, 0);
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
    struct NET_CLOSE_CONNECTION_PDU net_close_connection;
    net_close_connection.type = NET_CLOSE_CONNECTION;
    char net_close_connection_buffer[sizeof(net_close_connection)];
    int net_close_connection_size = serialize_net_close_connection(&net_close_connection, net_close_connection_buffer);

    printf("Sending NET_CLOSE_CONNECTION to the current successor...\n");
    // I think we shall close the file descriptor that is connected to the successor.
    // I mean close the socket that is connected to the successor.

    // Send the NET_CLOSE_CONNECTION PDU to the successor
    int send_status = send(node->sockfd_b, net_close_connection_buffer, net_close_connection_size, 0);
    if (send_status == -1) {
        perror("send failure");
        return 1;
    }

    // connect to the prospect aka the new node
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = net_join->src_address; // net_join messacge is host order.
    addr.sin_port = htons(net_join->src_port);
    // close the previous socket connection
    shutdown(node->sockfd_b, SHUT_RDWR);
    close(node->sockfd_b);
    //free(data);
    

    // creating a new socket for the new successor.
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

    uint8_t successor_start, successor_end;
    // now we will transfer the upper half of the hash range to the successor.
    split_range(node, &successor_start, &successor_end);
    printf("The node's updated hash range is: (%d, %d)\n", node->hash_range_start, node->hash_range_end);
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
    printf("The node's hash span: %d\n", node->hash_span);

    // Prepare the NET_JOIN_RESPONSE PDU    
    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};
    net_join_response.type = NET_JOIN_RESPONSE;
    net_join_response.next_address = node->public_ip.s_addr;
    net_join_response.next_port = htons(node->port); // htons(node->port); 
    net_join_response.range_start = successor_start;
    net_join_response.range_end = successor_end;

    char net_join_response_buffer[sizeof(net_join_response)];
    size_t bytes_to_send_net_join = serialize_net_join_response(&net_join_response, net_join_response_buffer);


    // Send the NET_JOIN_RESPONSE PDU to the successor aka to the prospect.
    send_status = send(node->sockfd_b, net_join_response_buffer, bytes_to_send_net_join, 0);
    if (send_status == -1) {
        perror("send failure");
        return 1;
    }

    // we are now assining the new successor to the node.
    node->successor_ip_address = addr.sin_addr;
    node->successor_port = net_join->src_port;
    printf("Updated successor is %s: %d\n", inet_ntoa(node->successor_ip_address), node->successor_port);
    printf("The successor's hash range start: %d, end: %d\n", successor_start, successor_end);

    // we are about to transfer upper half of the hash range to the successor.
    transfer_upper_half(node, successor_start, successor_end);
    
    // Move to the next state (q6)
    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}


int q17_state(void* n, void* data) {
    printf("[q17 state]\n");
    Node* node = (Node*)n;

    // Close the previous socket connection aka the connection with the predecessor.
    if(node->sockfd_d >= 0){
        shutdown(node->sockfd_d, SHUT_RDWR);
        close(node->sockfd_d);
        node->sockfd_d = -1;
        node->predecessor_ip_address.s_addr = INADDR_NONE;
        node->predecessor_port = 0;
    }

    // Here we assume node->successor_ip_address and node->successor_port
    // are already set in a consistent manner (host order for port).
    // If node->successor_port was not previously converted to host order,
    // make sure to do so at the time you set it.
    printf("the node's hash range start: %d, end: %d\n", node->hash_range_start, node->hash_range_end);
    // Check if this is the only node in the network
    if (node->hash_range_start == 0 && node->hash_range_end == 255) {
        printf("I am the only node in the network. Moving to state Q6...\n");
        node->state_handler = state_handlers[STATE_6];
        node->state_handler(node, NULL);
        return 0;
    }

    // Accept the new predecessor
    struct sockaddr_in predecessor_addr = {0};
    socklen_t addr_len = sizeof(predecessor_addr);

    printf("Accepting new predecessor...\n");

    //TODO: we may need to add fctl to set the socket to non-blocking
    int accept_status = accept(node->listener_socket, (struct sockaddr*)&predecessor_addr, &addr_len);
    if (accept_status == -1) {
        perror("accept failed");
        return 1;
    }

    // Store predecessor IP and port
    // Do not convert IP to host order; inet_ntoa expects network order.
    node->predecessor_ip_address = predecessor_addr.sin_addr;
    // Convert predecessor port to host order right away
    node->predecessor_port = ntohs(predecessor_addr.sin_port);
    node->sockfd_d = accept_status;
    printf("The node's successor is %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);
    printf("Accepted new predecessor: %s:%d\n", inet_ntoa(node->predecessor_ip_address),
           node->predecessor_port);


    return 0;
}

size_t serialize_net_join_response(const struct NET_JOIN_RESPONSE_PDU* pdu, char* buffer){
    size_t offset = 0;
    buffer[offset] = pdu->type;
    offset += sizeof(pdu->type);

    memcpy(buffer + offset, &pdu->next_address, sizeof(pdu->next_address));
    offset += sizeof(pdu->next_address);

    memcpy(buffer + offset, &pdu->next_port, sizeof(pdu->next_port));
    offset += sizeof(pdu->next_port);

    buffer[offset] = pdu->range_start;
    offset += sizeof(pdu->range_start);

    buffer[offset] = pdu->range_end;
    offset += sizeof(pdu->range_end);

    return offset;
}
