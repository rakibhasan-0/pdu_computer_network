#include "signal_handler.h"


// that function intends to close the connection by closing all the sockets.
static void close_connection (Node* node){
    
    if (node->sockfd_a >= 0) {
        if (close(node->sockfd_a) == -1) {
            perror("close sockfd_a failed");
        }
    }
    if (node->listener_socket >= 0) {
        if (close(node->listener_socket) == -1) {
            perror("close listener_socket failed");
        }
    }
    if (node->sockfd_b >= 0) {
        if (close(node->sockfd_b) == -1) {
            perror("close sockfd_b failed");
        }
    }
    if (node->sockfd_d >= 0) {
        if (close(node->sockfd_d) == -1) {
            perror("close sockfd_d failed");
        }
    }

}


/**
 * this function initialy checks if the node is empty or not.
 * if the node is empty it will simple close the sockets and exit.
 * if not then it will move to the q11 state.
 */
int q10_state(void* n, void* data) {

    Node* node = (Node*)n;
    printf("\n[q10 state]\n");

    if(node->successor_ip_address.s_addr == INADDR_NONE && node->successor_port == 0){
        printf("I am the only node in the network. Exiting...\n");
        close_connection(node);
        printf("I am Exiting.\n");
        exit(0);      
    }
  
    // if the node is not empty, then we will move to the another state.
    node->state_handler = state_handlers[STATE_11];
    node->state_handler(node, NULL);
    
    return 0;
}


// that function basically handles the signal, by updating the should_close variable to 1.
void signal_handler(int signum) {
    if(should_close == 0){
        should_close = 1;
    }
}

// this function will register the signal handlers.
void register_signal_handlers(){
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    // we are registering the signal handlers for the following signals.
    // these signals are SIGINT, SIGTERM, SIGQUIT.
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}

// state 11, we will handle that state when there are predecessor and successor for the node.

int q11_state(void* n, void* data){
    printf("[q11 state]\n");

    Node* node = (Node*)n;
    // we will send NET_NEW_RING message to the successor.
    // even though in the state diagaram it stated that one can send the message either
    // to the predecessor or successor.
    // but sending the message to the successor is more intuitive.
    struct NET_NEW_RANGE_PDU net_new_range = {0};
    net_new_range.type = NET_NEW_RANGE;
    net_new_range.range_start = node->hash_range_start;
    net_new_range.range_end = node->hash_range_end;

    // we are sending the NET_NEW_RANGE message to the successor.
    int send_status = send(node->sockfd_b, &net_new_range, sizeof(net_new_range), 0);
    printf("Sending NET_NEW_RANGE message to the successor\n");
    if (send_status == -1){
        perror("send failed");
        return 1;
    }

    printf("the successor is %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);
    // here we will receive the NET_NEW_RANGE_RESPONSE message from the successor.
    struct NET_NEW_RANGE_RESPONSE_PDU net_new_range_response = {0};
    int recv_status = recv(node->sockfd_b, &net_new_range_response, sizeof(net_new_range_response), 0);
    if (recv_status == -1){
        perror("recv failed");
        return 1;
    }

    if(net_new_range_response.type == NET_NEW_RANGE_RESPONSE){
        printf("now we are moving to the state 18\n");
        node->state_handler = state_handlers[STATE_18];
        node->state_handler(node, NULL);
    }

    return 0;
}



// state 15, where we will update the hash range of the successor.
int q15_state(void* n, void* data){

    printf("[q15 state]\n");
    printf("we are in the state 15\n");
    Node* node = (Node*)n;
    struct NET_NEW_RANGE_PDU* net_new_range = (struct NET_NEW_RANGE_PDU*)data;

    printf("Received NET_NEW_RANGE message from the predecessor\n");
    printf("New range start: %d\n", net_new_range->range_start);
    printf("New range end: %d\n", net_new_range->range_end);
    printf("the node's rnanage start is %d\n", node->hash_range_start);
    printf("the node's range end is %d\n", node->hash_range_end);
    

    // now we will invoke the fucntion to update the hash range of the node.
    update_hash_range(node, net_new_range->range_start, net_new_range->range_end);

    // now we will send NET_NEW_RANGE_RESPONSE message to the predecessor.
    struct NET_NEW_RANGE_RESPONSE_PDU net_new_range_response = {0};
    net_new_range_response.type = NET_NEW_RANGE_RESPONSE;

    printf("the prededcessor socket is %d\n", node->sockfd_d);
    printf("the predecessor is %s:%d\n", inet_ntoa(node->predecessor_ip_address), node->predecessor_port);
    // we are sending the NET_NEW_RANGE_RESPONSE message to the predecessor.
    int send_status = send(node->sockfd_d, &net_new_range_response, sizeof(net_new_range_response), 0);
    printf("Sending NET_NEW_RANGE_RESPONSE message to the predecessor\n");
    if (send_status == -1){
        perror("send failed");
        return 1;
    }

    printf("now we are moving to the state 6\n");
    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}



// the Q18 state, where we will close the connection with the predecessor and successor and exit gracefully.
int q18_state(void* n, void* data){

    printf("[q18 state]\n");

    // as we have updated the hash range to the node's successor, I mean we have mereged the hash range.
    // here we will transfer the data to the successor.

    // data transfer to the successor shall be implemented here, I mean all the data shall be transferred to the successor.

    Node* node = (Node*)n;
    // now we will send NET_CLOSE_CONNECTION message to the successor.
    struct NET_CLOSE_CONNECTION_PDU net_close_connection = {0};
    net_close_connection.type = NET_CLOSE_CONNECTION;
    int send_status = send(node->sockfd_b, &net_close_connection, sizeof(net_close_connection), 0);
    printf("Sending NET_CLOSE_CONNECTION message to the successor\n");
    if (send_status == -1){
        perror("send failed");
        return 1;
    }


    // now we will send NET_LEAVING message to the predecessor.
    // in that message we will add the new successor's address and port.
    struct NET_LEAVING_PDU net_leaving = {0};
    net_leaving.type = NET_LEAVING;
    net_leaving.new_address = node->successor_ip_address.s_addr; // it is already in the network order.
    net_leaving.new_port = htons(node->successor_port); // we need to convert it to the network order.

    int send_leaving_status = send(node->sockfd_d, &net_leaving, sizeof(net_leaving), 0);
    printf("Sending NET_LEAVING message to the predecessor\n");
    if (send_leaving_status == -1){
        perror("send failed");
        return 1;
    }

    // now we will close the connection with the predecessor and successor.
    close_connection(node);
    exit(0);
}


// the Q16 state, where the node will receive the NET_LEAVING message from the leaving node.
int q16_state(void* n, void* data){
    printf("[q16 state]\n");
    Node* node = (Node*)n;
    struct NET_LEAVING_PDU* net_leaving = (struct NET_LEAVING_PDU*)data;

    printf("Received NET_LEAVING message from the leaving node\n");
    printf("Old successor: %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);
    net_leaving->new_port = ntohs(net_leaving->new_port);
    net_leaving->new_address = net_leaving->new_address;


    // now we will close the connection with the successor.
    if (node->sockfd_b >= 0) {
        // first shutdown the socket
        if(shutdown(node->sockfd_b, SHUT_RDWR) == -1){
            perror("shutdown failed");
        }
        // then close the socket
        if(close(node->sockfd_b) == -1){
            perror("close failed");
        }
    }

    // now we are checking if the new successor is the current node itself.    
    if(net_leaving->new_address == node->public_ip.s_addr && net_leaving->new_port == node->port){
        node->successor_ip_address.s_addr = INADDR_NONE;
        node->successor_port = 0;
        node->sockfd_b = -1;
        printf("now we will move to state 6\n");
        node->state_handler = state_handlers[STATE_6];
        node->state_handler(node, NULL);
    }
    else {

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = net_leaving->new_address; // it is already in the network order
        addr.sin_port = htons(net_leaving->new_port);
        node->sockfd_b = socket(AF_INET, SOCK_STREAM, 0);

        int connect_status = connect(node->sockfd_b, (struct sockaddr*)&addr, sizeof(addr));
        if (connect_status == -1) {
            perror("connect failure");
            return 1;
        }

        // now we will update the successor's address and port.
        node->successor_ip_address.s_addr = net_leaving->new_address;
        node->successor_port = net_leaving->new_port;

        printf("New successor: %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);
        printf("now we are moving to the state 6\n");
        node->state_handler = state_handlers[STATE_6];
        node->state_handler(node, NULL);

    }

    return 0;

}