
#include "communication.h"

// the state will recieve the NET_JOIN_RESPONSE from the state 7,
// it contains the information about the next node in the network.



// that state will extract the information from the NET_JOIN_PDU and store it in the node.
// it will also create a socket for the B endpoint communication.
// as in the pdu structure states that we update the pdu max fields... 
int q12_state(void* n, void* data) {
    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;
    printf("q12 state\n");

    // we are checking the message we got from the state 6
    printf("Type__rcv from 6: %d\n", net_join->type);
    printf("Source Address__rcv from 6: %s\n", inet_ntoa((struct in_addr){.s_addr = net_join->src_address}));
    printf("Source Port:__rcv from 6 %d\n", net_join->src_port);
    printf("Max Span:___rcv from 6 %d\n", net_join->max_span);
    printf("Max Address:___rcv from 6 %s\n", inet_ntoa((struct in_addr){.s_addr = net_join->max_address}));
    printf("Max Port:___ rcv from 6 %d\n", net_join->max_port);
    // updating the max fields of the net_join message.

    net_join->max_address = node->public_ip.s_addr;
    net_join->max_port = node->port;
    net_join->max_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);

    // checking the updated net_join message
    printf("Type__after__updated: %d\n", net_join->type);
    printf("Source Address__after__updated: %s\n", inet_ntoa((struct in_addr){.s_addr = net_join->src_address}));
    printf("Source Port:__after__updated %d\n", net_join->src_port);
    printf("Max Span:___after__updated %d\n", net_join->max_span);
    printf("Max Address:___after__updated %s\n", inet_ntoa((struct in_addr){.s_addr = net_join->max_address}));
    printf("Max Port:___after__updated %d\n", net_join->max_port);



    // Check if the node has no successor or predecessor
    if (node->predecessor_ip_address.s_addr == INADDR_NONE && node->successor_ip_address.s_addr == INADDR_NONE) {
        // Move to Q5 state
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

    printf("The successor port is %d\n", node->successor_port);
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

    printf("Successfully connected to successor: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    int send_status = send(node->sockfd_b, &net_join_response, sizeof(net_join_response), 0);
    if (send_status == -1) {
        perror("send failure");
        return 1;
    }

    printf("Sent NET_JOIN_RESPONSE to successor.\n");
    node->hash_range_start = 0;  // Just for testing purposes
    node->hash_range_end = 127;  // Just for testing purposes
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);

    printf("Assigned hash range: Start=%d, End=%d, Span=%d\n", node->hash_range_start, node->hash_range_end, node->hash_span);

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

    printf("Accepted predecessor: %s:%d\n", inet_ntoa(predecessor_addr.sin_addr), ntohs(predecessor_addr.sin_port));

    // Transition to Q6 after accepting predecessor
    printf("Transitioning to state Q6...\n");
    node->state_handler = state_handlers[5];
    node->state_handler(node, NULL);

    return 0;
}
