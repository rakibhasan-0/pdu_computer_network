#include "join_network.h"

// state number 4, we dont know anything about this state yet.
// 
int q4_state(void* n, void* data){

    Node* node = (Node*)n;
    printf("q4 state\n");
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
    printf("q7 state\n");

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

    printf("we are about to send the net_join to the state 6\n");
    printf("we are using UDP to send the net_join\n");
    ssize_t bytes_sent = sendto(node->sockfd_a, &net_join, sizeof(net_join), 0, (struct sockaddr *)&addr, sizeof(addr));
    if (bytes_sent == -1) {
        perror("sendto failed");
        return 1;
    }

    printf("Sent NET_JOIN from state_7\n");
    // we will listen to the NET_JOIN_RESPONSE from the node
    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};
 
    
    // we are using it to get the senders port and address as net_join_response.
    // does not contain the information about the sender aka the predecessor.
    struct sockaddr_in sender_addr; 
    printf("the listner port is %d\n", node->port);
    socklen_t sender_addr_len = sizeof(sender_addr); 
    int accept_status = accept(node->listener_socket, (struct sockaddr*)&sender_addr, &sender_addr_len);
    printf("the new socket for the predecessor is _________ %d\n", accept_status);
    if (accept_status == -1) {
        perror("accept failed");
        return 1;
    }

    printf("we are about to recieve the NET_JOIN_RESPONSE from the state 5\n");
    int recv_status = recv(accept_status, &net_join_response, sizeof(net_join_response), 0);
    if(recv_status == -1){
        perror("recv failed");
        return 1;
    }
    printf("Received NET_JOIN_RESPONSE\n");
    printf("The number of bytes received: %d\n", recv_status);
    printf("Next Address: %s\n", inet_ntoa((struct in_addr){.s_addr = ntohl(net_join_response.next_address)}));
    printf("Next Port: %d\n", ntohs(net_join_response.next_port)); 
    printf("Range Start: %d\n", net_join_response.range_start);
    printf("Range End: %d\n", net_join_response.range_end);

    // accept predecessor means the src address of the net_join_response is the predecessor of the node.
    node->predecessor_ip_address.s_addr = ntohl(net_join_response.next_address);
    node->predecessor_port = ntohs(net_join_response.next_port); // we need to think about the port number of the predecessor...
    node->successor_ip_address.s_addr = net_get_node_response->address;
    node->successor_port = ntohs(net_get_node_response->port);
    node->hash_range_start = net_join_response.range_start;
    node->sockfd_d = accept_status;
    node->hash_range_end = net_join_response.range_end;
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
    printf("Predecessor IP: %s\n", inet_ntoa(node->predecessor_ip_address)); // it is not the original port..butit seems there is another port.
    printf("Predecessor Port: %d\n", node->predecessor_port);
    printf("Successor IP: %s\n", inet_ntoa(node->successor_ip_address));
    printf("Successor Port: %d\n", node->successor_port);
    printf("we are about to move q8 state from q7\n");
    
    //close(node->listener_socket);
    //node->listener_socket = 0;

    // we will move to the q8 state and connect to the successor.
    // the question is what does mean by that, I mean how can I connect with that 
    node->state_handler = state_handlers[8];
    node->state_handler(node, &net_join_response);


    return 0;
}


int q8_state(void* n, void* data) {
    printf("q8 state\n");
    Node* node = (Node*)n;

    // Prepare the GET_SUCCESSOR_PDU request
    struct GET_SUCCESSOR_PDU get_successor = {0};
    get_successor.type = GET_SUCCESSOR;

    printf("Sending GET_SUCCESSOR request to predecessor.\n");
    printf("Predecessor IP: %s\n", inet_ntoa(node->predecessor_ip_address));
    printf("Predecessor Port: %d\n", node->predecessor_port);
   
    // Send GET_SUCCESSOR_PDU to the predecessor
    ssize_t bytes_sent = send(node->sockfd_d, &get_successor, sizeof(get_successor), 0);
    if (bytes_sent == -1) {
        perror("send to predecessor failed");
        close(node->sockfd_d);
        node->sockfd_d = -1;
        return -1;
    }

    printf("RHe is the predecessor socket,%d\n", node->sockfd_d);
    printf("GET_SUCCESSOR request sent to predecessor.\n");

    // Prepare to receive GET_SUCCESSOR_RESPONSE_PDU
    struct GET_SUCCESSOR_RESPONSE_PDU get_successor_response = {0};
    ssize_t bytes_received = recv(node->sockfd_d, &get_successor_response, sizeof(get_successor_response), 0);
    if (bytes_received == -1) {
        perror("recv from predecessor failed");
        close(node->sockfd_d);
        node->sockfd_d = -1;
        return -1;
    }

    printf("Received GET_SUCCESSOR_RESPONSE from predecessor.\n");
    printf("Successor IP: %s\n", inet_ntoa((struct in_addr){.s_addr = get_successor_response.successor_address}));
    printf("Successor Port: %d\n", ntohs(get_successor_response.successor_port));

    // Update the node's successor information
    node->successor_ip_address.s_addr = get_successor_response.successor_address;
    node->successor_port = ntohs(get_successor_response.successor_port);

    // Print the updated successor details
    printf("Updated Successor IP: %s\n", inet_ntoa(node->successor_ip_address));
    printf("Updated Successor Port: %d\n", node->successor_port);

    // Move to the next state (q6) or handle additional logic
    node->state_handler = state_handlers[5];
    node->state_handler(node, NULL);

    return 0;
}







