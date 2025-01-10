#include "join_network.h"

// state number 4, we dont know anything about this state yet.


static int serialize_net_join(struct NET_JOIN_PDU* pdu, char* buffer);
static void deserialize_net_join_response(struct NET_JOIN_RESPONSE_PDU* pdu, const char* buffer);

static int serialize_net_join(struct NET_JOIN_PDU* pdu, char* buffer){
    size_t offset = 0;
    memcpy(buffer + offset, &pdu->type, sizeof(pdu->type));
    offset += sizeof(pdu->type);
    memcpy(buffer + offset, &pdu->src_address, sizeof(pdu->src_address));
    offset += sizeof(pdu->src_address);

    pdu->src_port = htons(pdu->src_port);
    memcpy(buffer + offset, &pdu->src_port, sizeof(pdu->src_port));
    offset += sizeof(pdu->src_port);

    memcpy(buffer + offset, &pdu->max_span, sizeof(pdu->max_span));
    offset += sizeof(pdu->max_span);
    memcpy(buffer + offset, &pdu->max_address, sizeof(pdu->max_address));
    offset += sizeof(pdu->max_address);
    
    pdu->max_port = htons(pdu->max_port);
    memcpy(buffer + offset, &pdu->max_port, sizeof(pdu->max_port));
    offset += sizeof(pdu->max_port);
    return offset;
}


int q4_state(void* n, void* data){

    Node* node = (Node*)n;
    printf("[Q4 state]\n");
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
    printf("[Q7 state]\n");
    printf("I am not the first node, sending NET_JOIN message...\n");

    struct NET_GET_NODE_RESPONSE_PDU* net_get_node_response = (struct NET_GET_NODE_RESPONSE_PDU*)data;
    Node* node = (Node*)n;

    // Construct the NET_JOIN_PDU to send
    struct NET_JOIN_PDU net_join = {0};
    net_join.type = NET_JOIN;

    // As we have already received host order or converted to host order in q3_state
    //printf("if we ntohs the port %d\n", ntohs(node->port));
    //printf("if we htonl the port %d\n", htons(node->port));

    net_join.src_address = node->public_ip.s_addr;  
    net_join.src_port = node->port;
    net_join.max_span = 0;  
    net_join.max_address = 0; // If you have a known address, use htonl() here.
    net_join.max_port = 0;    // If you have a known port, use htons() here.

    // we will seraialize the NET_JOIN_PDU
    char buffer[sizeof(struct NET_JOIN_PDU)];
    memset(buffer, 0, sizeof(buffer));
    size_t bytes_to_be_send = serialize_net_join(&net_join, buffer);

    // Prepare to send to the node given by net_get_node_response
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = net_get_node_response->address;  // from the state 3 we are assuming that the address is in the network order.
    addr.sin_port = htons(net_get_node_response->port);

    // Send the NET_JOIN message (UDP )
    ssize_t bytes_sent = sendto(node->sockfd_a, buffer, bytes_to_be_send, 0, (struct sockaddr*)&addr, sizeof(addr));
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
    node->predecessor_port = sender_addr.sin_port;
    node->sockfd_d = accept_status;
    printf("Accepted new predecessor: %s:%d\n", inet_ntoa(node->predecessor_ip_address), node->predecessor_port);

    // Receive the NET_JOIN_RESPONSE_PDU from predecessor
    char buffer_net_join_response[sizeof(struct NET_JOIN_RESPONSE_PDU)];
    memset(buffer_net_join_response, 0, sizeof(buffer_net_join_response));

    ssize_t recv_status = recv(accept_status, buffer_net_join_response, sizeof(buffer_net_join_response), 0);
    if (recv_status == -1) {
        perror("recv failed");
        return 1;
    } 

    // --- Allocate a NET_JOIN_RESPONSE_PDU on the heap ---
    struct NET_JOIN_RESPONSE_PDU* net_join_response
        = malloc(sizeof(struct NET_JOIN_RESPONSE_PDU)); // we can use stack memory here.
    if (!net_join_response) {
        perror("malloc failed");
        return 1;
    }
    memset(net_join_response, 0, sizeof(*net_join_response));

    // --- Deserialize into the struct ---
    deserialize_net_join_response(net_join_response, buffer_net_join_response);
    //printf("the net join response port is %d\n", net_join_response->next_port);

    printf("Received NET_JOIN_RESPONSE from predecessor\n");
    node->hash_range_start= net_join_response->range_start;
    node->hash_range_end = net_join_response->range_end;
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);

    //printf("the port from the net join response is %d\n", net_join_response->next_port);
    //printf("the port after being converted to host order %d\n", net_join_response->next_port);
 
    // Move to q8_state
    node->state_handler = state_handlers[STATE_8];
    node->state_handler(node, net_join_response);

    return 0;
}



int q8_state(void* n, void* data) {
    printf("[Q8 state]\n");

    Node* node = (Node*)n;
    struct NET_JOIN_RESPONSE_PDU* net_join_response = (struct NET_JOIN_RESPONSE_PDU*)data;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = net_join_response->next_address;
    addr.sin_port = ntohs(net_join_response->next_port);

    printf("Connecting to the successor...\n");
    printf("The address of the successor is %s\n", inet_ntoa(addr.sin_addr));
    printf("The port of the successor is %d\n", ntohs(addr.sin_port));

    node->sockfd_b = socket(AF_INET, SOCK_STREAM, 0);
    if (node->sockfd_b == -1) {
        perror("socket failure");
        free(net_join_response);
        return 1;
    }

    printf("the address of the successor is %s\n", inet_ntoa(addr.sin_addr));
    printf("the port of the successor is %d\n", ntohs(addr.sin_port));
    if (connect(node->sockfd_b, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect failure");
        free(net_join_response);
        close(node->sockfd_b);
        return 1;
    }

    node->successor_ip_address = addr.sin_addr;
    node->successor_port = net_join_response->next_port;
	
	//printf("Freeing net_join_response in join_network.c\n");
    free(net_join_response);

    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}

static void deserialize_net_join_response(struct NET_JOIN_RESPONSE_PDU* pdu, const char* buffer){
    //printf("Deserializing NET_JOIN_RESPONSE_PDU\n");
    size_t offset = 0;
    pdu->type = buffer[offset];
    offset += sizeof(pdu->type);
    memcpy(&pdu->next_address, buffer + offset, sizeof(pdu->next_address));
    offset += sizeof(pdu->next_address);
    memcpy(&pdu->next_port, buffer + offset, sizeof(pdu->next_port));
    offset += sizeof(pdu->next_port);
    pdu->next_port = ntohs(pdu->next_port);
    pdu->range_start = buffer[offset];
    offset += sizeof(pdu->range_start);
    pdu->range_end = buffer[offset];
    offset += sizeof(pdu->range_end);
}

