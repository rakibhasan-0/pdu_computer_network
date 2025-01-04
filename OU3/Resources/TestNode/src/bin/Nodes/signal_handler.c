#include "signal_handler.h"





size_t construct_net_new_range(struct NET_NEW_RANGE_PDU* pdu, char* buffer, Node* node){ 

    size_t offset = 0; 
    buffer[offset++] = pdu->type;
    buffer[offset++] = node->hash_range_start;
    buffer[offset++] = node->hash_range_end;

    return offset;

}

size_t serialize_net_leaving (const struct NET_LEAVING_PDU* pdu, char* buffer){
    size_t offset = 0;
    buffer[offset++] = pdu->type; // 1 byte

    memcpy(buffer + offset, &pdu->new_address, sizeof(pdu->new_address));
    offset += sizeof(pdu->new_address); // 4 bytes

    uint16_t new_port = htons(pdu->new_port);
    memcpy(buffer + offset, &new_port, sizeof(new_port));
    offset += sizeof(new_port); // 2 bytes
    return offset;
}










// that function intends to close the connection by closing all the sockets.
static void close_connection (Node* node){
    
    if (node->sockfd_a > 0) {
        if (close(node->sockfd_a) == -1) {
            perror("close sockfd_a failed");
        }
    }
    if (node->listener_socket > 0) {
        if (close(node->listener_socket) == -1) {
            perror("close listener_socket failed");
        }
    }
    if (node->sockfd_b > 0) {
        if (close(node->sockfd_b) == -1) {
            perror("close sockfd_b failed");
        }
    }
    if (node->sockfd_d > 0) {
        if (close(node->sockfd_d) == -1) {
            perror("close sockfd_d failed");
        }
    }

    // we may need to deallocate the memory here.
    destroy_allocated_memory(node);

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
        // we may need deallocate everything here.
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

int q11_state(void* n, void* data) {
    printf("[q11 state]\n");

    Node* node = (Node*)n;

    // Preparing NET_NEW_RANGE message
    struct NET_NEW_RANGE_PDU net_new_range = {0};
    net_new_range.type = NET_NEW_RANGE;
    
    char buffer[sizeof(net_new_range)];
    
    size_t size_to_send = construct_net_new_range(&net_new_range, buffer, node);

    // Now we are deciding whether to send the message to the successor or the predecessor.
    bool send_to_successor = node->hash_range_start == 0;

    // Send NET_NEW_RANGE message
    int send_status;
    if (send_to_successor) {
        printf("Sending NET_NEW_RANGE message to the successor\n");
        send_status = send(node->sockfd_b, buffer, size_to_send, 0);
        if (send_status == -1) {
            perror("send to successor failed");
            return 1;
        }
        printf("The successor is %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);
    } else {
        printf("Sending NET_NEW_RANGE message to the predecessor\n");
        send_status = send(node->sockfd_d,buffer,size_to_send, 0);
        if (send_status == -1) {
            perror("send to predecessor failed");
            return 1;
        }
        printf("The predecessor is %s:%d\n", inet_ntoa(node->predecessor_ip_address), node->predecessor_port);
    }

    // Receive NET_NEW_RANGE_RESPONSE
    struct NET_NEW_RANGE_RESPONSE_PDU net_new_range_response = {0};
    printf("Waiting for NET_NEW_RANGE_RESPONSE...\n");

    int recv_status;
    // flush the socket buffer
    if (send_to_successor) {
        recv_status = recv(node->sockfd_b, &net_new_range_response, sizeof(net_new_range_response), 0);
        

    } else {
        recv_status = recv(node->sockfd_d, &net_new_range_response, sizeof(net_new_range_response), 0);
    }

    if (recv_status == -1) {
        perror("recv failed");
        return 1;
    }

    printf("Received NET_NEW_RANGE_RESPONSE\n");
    printf("net_new_range_response type: %d\n", net_new_range_response.type);

    if (net_new_range_response.type == NET_NEW_RANGE_RESPONSE) {
        printf("Received NET_NEW_RANGE_RESPONSE. Moving to STATE_18.\n");
        node->state_handler = state_handlers[STATE_18];
        node->state_handler(node, NULL);
    }
    return 0;
}

// state 15, where we will update the hash range dynamically and decide the response target.
int q15_state(void* n, void* data) {

    printf("[q15 state]\n");

    Node* node = (Node*)n;
    struct NET_NEW_RANGE_PDU* net_new_range = (struct NET_NEW_RANGE_PDU*)data;

    //printf("Received NET_NEW_RANGE message\n");
    //printf("New range start: %d\n", net_new_range->range_start);
    //printf("New range end: %d\n", net_new_range->range_end);
    //printf("Node's current range: (%d, %d)\n", node->hash_range_start, node->hash_range_end);

    // Determine whether to send the response to the successor or the predecessor
    bool send_to_successor = (node->hash_range_end != 255 && net_new_range->range_start == node->hash_range_end + 1);

    if (send_to_successor) {
        // Update hash range for successor
        printf("Sending NET_NEW_RANGE_RESPONSE to the successor\n");
        update_hash_range(node, node->hash_range_start, net_new_range->range_end);
        char buffer[sizeof(struct NET_NEW_RANGE_RESPONSE_PDU)];
        struct NET_NEW_RANGE_RESPONSE_PDU net_new_range_response = {0};
        buffer[0] = NET_NEW_RANGE_RESPONSE;



        //printf("The successor is %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);
        int send_status = send(node->sockfd_b, buffer, sizeof(buffer), 0);
        if (send_status == -1) {
            perror("Send failed");
            return 1;
        }
    } else {
        // Update hash range for predecessor
        printf("Sending NET_NEW_RANGE_RESPONSE to the predecessor\n");
        update_hash_range(node, net_new_range->range_start, node->hash_range_end);

        struct NET_NEW_RANGE_RESPONSE_PDU net_new_range_response = {0};
        net_new_range_response.type = NET_NEW_RANGE_RESPONSE;
        char new_range_response_buffer[sizeof(struct NET_NEW_RANGE_RESPONSE_PDU)];
        new_range_response_buffer[0] = net_new_range_response.type;

        //printf("The predecessor is %s:%d\n", inet_ntoa(node->predecessor_ip_address), node->predecessor_port);
        int send_status = send(node->sockfd_d, new_range_response_buffer, sizeof(net_new_range_response), 0);
        if (send_status == -1) {
            perror("Send failed");
            return 1;
        }
    }

    // Log the updated hash range
    printf("Updated node's range: (%d, %d)\n", node->hash_range_start, node->hash_range_end);

    return 0;
}


// the Q18 state, where we will close the connection with the predecessor and successor and exit gracefully.
int q18_state(void* n, void* data){

    printf("[q18 state]\n");

    Node* node = (Node*)n;

    // as we have updated the hash range to the node's successor, I mean we have mereged the hash range.
    // here we will transfer the data to the successor.
    bool to_successor = false;
    if(node->hash_range_start == 0){
        to_successor = true;
    }

   // we are transferring the entries to the successor/predecessor.
    transfer_all_entries(node, to_successor);

    // now we will send NET_CLOSE_CONNECTION message to the successor.
    struct NET_CLOSE_CONNECTION_PDU net_close_connection = {0};
    net_close_connection.type = NET_CLOSE_CONNECTION;
    // serialize the net_close_connection to the buffer.
    char buffer_net_close[sizeof(struct NET_CLOSE_CONNECTION_PDU)];
    buffer_net_close[0] = net_close_connection.type;

    int send_status = send(node->sockfd_b, buffer_net_close, sizeof(buffer_net_close), 0);
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
    net_leaving.new_port = node->successor_port; // we need to convert it to the network order.
    // here we will add the net_leaving to the buffer to seralize it.
    char net_leaving_buffer [sizeof(struct NET_LEAVING_PDU)];
    memset(net_leaving_buffer, 0, sizeof(net_leaving_buffer));

    int bytes_to_send = serialize_net_leaving(&net_leaving, net_leaving_buffer);
    int send_leaving_status = send(node->sockfd_d, net_leaving_buffer, bytes_to_send, 0);
    
    printf("Sending NET_LEAVING message to the predecessor\n");
    if (send_leaving_status == -1){
        perror("send failed");
        return 1;
    }

    printf("Exiting...\n");

    // now we will close the connection with the predecessor and successor.
    close_connection(node);
    exit(0);
}

// The Q16 state, where the node will receive the NET_LEAVING message from the leaving node.
int q16_state(void* n, void* data){
    printf("[q16 state]\n");
    Node* node = (Node*)n;
    struct NET_LEAVING_PDU* net_leaving = (struct NET_LEAVING_PDU*)data;

    printf("Received NET_LEAVING message from the leaving node\n");
    printf("Old successor: %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);

    // Ensure that 'new_address' is in host byte order
    // If deserialization already converted it, no need to convert again
    // If not, apply ntohl here
    // Assuming deserialization handled byte order correctly

    // Prepare 'new_address' in network byte order for inet_ntoa or inet_ntop
    struct in_addr new_successor_addr;
    new_successor_addr.s_addr = net_leaving->new_address; // Convert to network byte order

    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &new_successor_addr, ip_str, sizeof(ip_str)) == NULL) {
        perror("inet_ntop failed");
        // Handle error accordingly, possibly return or set a default value
        strcpy(ip_str, "Invalid IP");
    }

    printf("New successor: %s:%d\n", ip_str, net_leaving->new_port);

    // Close the existing connection with the old successor if it's open
    if (node->sockfd_b >= 0) {
        // First, shutdown the socket to stop any further communication
        if(shutdown(node->sockfd_b, SHUT_RDWR) == -1){
            perror("shutdown failed");
            // Depending on requirements, you might choose to continue or handle the error
        }

        // Then, close the socket to free the file descriptor
        if(close(node->sockfd_b) == -1){
            perror("close failed");
            // Depending on requirements, you might choose to continue or handle the error
        }

        // Reset the socket file descriptor
        node->sockfd_b = -1;
    }

    // Check if the new successor is the current node itself
    if(net_leaving->new_address == node->public_ip.s_addr && net_leaving->new_port == node->port){
        node->successor_ip_address.s_addr = INADDR_NONE;
        node->successor_port = 0;
        node->sockfd_b = -1;
        printf("Now moving to state 6 as there is no successor.\n");
    }
    else {
        // Set up the sockaddr_in structure for the new successor
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = net_leaving->new_address; // Already in network byte order
        addr.sin_port = htons(net_leaving->new_port);    // Ensure port is in network byte order

        // Create a new socket for the new successor
        node->sockfd_b = socket(AF_INET, SOCK_STREAM, 0);
        if (node->sockfd_b == -1) {
            perror("socket creation failed");
            // Depending on requirements, handle the failure (e.g., retry, exit, notify)
            return 1;
        }

        // Attempt to connect to the new successor
        int connect_status = connect(node->sockfd_b, (struct sockaddr*)&addr, sizeof(addr));
        if (connect_status == -1) {
            perror("connect failure");
            close(node->sockfd_b);
            node->sockfd_b = -1;
            return 1;
        }

        // Update the node's successor information
        node->successor_ip_address.s_addr = net_leaving->new_address;
        node->successor_port = net_leaving->new_port;

        printf("Connected to new successor: %s:%d\n", ip_str, net_leaving->new_port);
    }
     
     printf("Moving to state 6\n");
    // Transition to state 6
    return 0;
}
