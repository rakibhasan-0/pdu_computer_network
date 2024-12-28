#include "join_network.h"

// state number 4, we dont know anything about this state yet.
// 
int q4_state(void* n, void* data){

    Node* node = (Node*)n;
    printf("[q4 state]\n");
    // the node itself will be its predecessor and successor
    // the initialization of the 
    node->predecessor_ip_address.s_addr = INADDR_NONE;
    node->successor_ip_address.s_addr = INADDR_NONE; // i am not usre whether both successor and predecessor would be null or itself.
    node->predecessor_port = 0;
    node->successor_port = 0;
    node->hash_range_start = 0;
    node->hash_range_end = 255; // since there is only one node.
    node->sockfd_b = -1;
    node->sockfd_d = -1;
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
    printf("Node's hash range: (%d, %d)\n", node->hash_range_start, node->hash_range_end);
    printf("Node's hash span: %d\n", node->hash_span);

    // start sending alive messages to the tracker
    node->state_handler = state_handlers[5]; // it will move to the q6 state
    node->state_handler(node, NULL);

    return 0;

}




// that state will NET_JOIN to the node that we got from the tracker.
// what I think i will start traversing the networ from the node that we got from the tracker.
// we will make the node the predecessor the founded node of the node.
int q7_state(void* n, void* data) {
    printf("[q7 state]\n");
    printf("I am not the first node, sending NET_JOIN message...\n");

    struct NET_GET_NODE_RESPONSE_PDU* net_get_node_response = (struct NET_GET_NODE_RESPONSE_PDU*)data;
    Node* node = (Node*)n;

    // Construct the NET_JOIN_PDU to send
    struct NET_JOIN_PDU net_join = {0};
    net_join.type = NET_JOIN;

    // As we have already received host order or converted to host order in q3_state
    net_join.src_address = node->public_ip.s_addr;  
    net_join.src_port = htons(node->port);
    net_join.max_span = 0;  
    net_join.max_address = 0; // If you have a known address, use htonl() here.
    net_join.max_port = 0;    // If you have a known port, use htons() here.

    // Prepare to send to the node given by net_get_node_response
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = net_get_node_response->address;  // from the state 3 we are assuming that the address is in the network order.
    addr.sin_port = htons(net_get_node_response->port);

    // Send the NET_JOIN message (UDP )
    ssize_t bytes_sent = sendto(node->sockfd_a, &net_join, sizeof(net_join), 0, (struct sockaddr *)&addr, sizeof(addr));
    if (bytes_sent == -1) {
        perror("sendto failed");
        return 1;
    }

    // we can free the memory now
    free(net_get_node_response);

    // Now we accept a new predecessor. The accept call returns a new socket.
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);

    int accept_status = accept(node->listener_socket, (struct sockaddr*)&sender_addr, &sender_addr_len);
    if (accept_status == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }else{
            printf("accept failed\n");
            return 1;
        }
    }

    // The sender_addr is in network order. Store predecessor's IP and port:
    // IP addresses in node->predecessor_ip_address in network order (direct assignment).
    // now we are storing the predecessor's ip address and port in the node as the host order.
    node->predecessor_ip_address = sender_addr.sin_addr; // we are storing the address 
    node->predecessor_port = ntohs(sender_addr.sin_port);
    node->sockfd_d = accept_status;
    printf("Accepted new predecessor: %s:%d\n", inet_ntoa(node->predecessor_ip_address), node->predecessor_port);

    // Receive the NET_JOIN_RESPONSE_PDU from predecessor
    struct NET_JOIN_RESPONSE_PDU* net_join_response = malloc(sizeof(struct NET_JOIN_RESPONSE_PDU));
    memset(net_join_response, 0, sizeof(struct NET_JOIN_RESPONSE_PDU));

    int recv_status = recv(accept_status, net_join_response, sizeof(*net_join_response), 0);
    if (recv_status == -1) {
        perror("recv failed");
        return 1;
    }
    // Converting all fields in NET_JOIN_RESPONSE_PDU to host order...
    net_join_response->next_address = net_join_response->next_address; // we are not converting the address to the host order, kept the network order, it is very unusual!!
    net_join_response->next_port = ntohs(net_join_response->next_port);
    net_join_response->range_start = net_join_response->range_start;
    net_join_response->range_end = net_join_response->range_end;



    printf("Received NET_JOIN_RESPONSE from predecessor\n");
    // we are updating the hash range of the node.
    node->hash_range_start = net_join_response->range_start;
    node->hash_range_end = net_join_response->range_end;
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
    printf("Node's updated range: (%d, %d)\n", node->hash_range_start, node->hash_range_end);
    printf("Node's updated hash range: %d\n", node->hash_span);

    // Move to q8_state to connect to the successor
    node->state_handler = state_handlers[STATE_8];
    node->state_handler(node, net_join_response);

    return 0;
}



int q8_state(void* n, void* data) {
    printf("[q8 state]\n");

    Node* node = (Node*)n;
    struct NET_JOIN_RESPONSE_PDU* net_join_response = (struct NET_JOIN_RESPONSE_PDU*)data;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = net_join_response->next_address;
    addr.sin_port = htons(net_join_response->next_port);

    node->sockfd_b = socket(AF_INET, SOCK_STREAM, 0);
    if (node->sockfd_b == -1) {
        perror("socket failure");
        free(net_join_response);
        return 1;
    }

    if (connect(node->sockfd_b, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect failure");
        free(net_join_response);
        close(node->sockfd_b);
        return 1;
    }

    node->successor_ip_address = addr.sin_addr;
    node->successor_port = net_join_response->next_port;

    free(net_join_response);

    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}
