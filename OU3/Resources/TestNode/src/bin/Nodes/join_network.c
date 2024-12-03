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
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);

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
    
    //printf("max port %d\n", ntohs(net_get_node_response->port));
    // Construct the NET_JOIN_PDU
    struct NET_JOIN_PDU net_join = {0};
    net_join.type = NET_JOIN; 
    net_join.src_address = node->public_ip.s_addr;  
    net_join.src_port = node->port;
    net_join.max_span = 0;  // as we dont anything about the max_span, max_address, max_port                       
    net_join.max_address = 0;
    net_join.max_port = 0; 

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = net_get_node_response->address; 
    addr.sin_port = net_get_node_response->port;

    //printf("we are about to send the net_join to the state 6\n");
    // the node will send the NET_JOIN to the node, one of the active node may accept the message 
    ssize_t bytes_sent = sendto(node->sockfd_a, &net_join, sizeof(net_join), 0, (struct sockaddr *)&addr, sizeof(addr));
    if (bytes_sent == -1) {
        perror("sendto failed");
        return 1;
    }

    //printf("Sent NET_JOIN from state_7\n");
    // we will listen to the NET_JOIN_RESPONSE from the node aka node which is the max node.
    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};   
    // we are using it to get the senders port and address as net_join_response.
    // does not contain the information about the sender aka the predecessor.
    struct sockaddr_in sender_addr; 
    //printf("the listner port is %d\n", node->port);

    // in the state machine diagram, it mentioned that we will accept the predcessor.
    // I guess this type of accepting in that case. 
    socklen_t sender_addr_len = sizeof(sender_addr); 
    int accept_status = accept(node->listener_socket, (struct sockaddr*)&sender_addr, &sender_addr_len);
    //printf("the new socket for the predecessor is _________ %d\n", accept_status);
    if (accept_status == -1) {
        perror("accept failed");
        return 1;
    }

      // accept predecessor means the src address of the net_join_response is the predecessor of the node.
    node->predecessor_ip_address.s_addr = ntohl(sender_addr.sin_addr.s_addr);
    node->predecessor_port = ntohs(sender_addr.sin_port);
    node->sockfd_d = accept_status;

    printf("Accepted new predecessor: %s:%d\n", inet_ntoa(sender_addr.sin_addr), ntohs(sender_addr.sin_port));

    // now we are receiving the NET_JOIN_RESPONSE from the node or the node that has max hash range.
    int recv_status = recv(accept_status, &net_join_response, sizeof(net_join_response), 0);
    if(recv_status == -1){
        perror("recv failed");
        return 1;
    }

   // we need to think about the port number of the predecessor...
    
    //close(node->listener_socket);
    //node->listener_socket = 0;

    // we will move to the q8 state and connect to the successor.
    // the question is what does mean by that, I mean how can I connect with that 
    node->state_handler = state_handlers[8];
    node->state_handler(node, &net_join_response);


    return 0;
}


int q8_state(void* n, void* data) {

    printf("[q8 state]\n");

    Node* node = (Node*)n;
    struct NET_JOIN_RESPONSE_PDU net_join_response = *(struct NET_JOIN_RESPONSE_PDU*)data;

    // connect to the successor
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ntohl(net_join_response.next_address);
    addr.sin_port = net_join_response.next_port;


    printf("Connecting to successor %s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    printf(" ...\n");

    // Connect to the successor
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

    printf("Connected to successor: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    
    // Move to the next state (q6) or handle additional logic
    node->state_handler = state_handlers[5];
    node->state_handler(node, NULL);

    return 0;
}







