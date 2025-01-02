#include "signal_handler.h"

size_t construct_net_new_range(struct NET_NEW_RANGE_PDU* pdu, char* buffer, Node* node){ 

    size_t offset = 0; 
    buffer[offset++] = pdu->type;
    buffer[offset++] = node->hash_range_start;
    buffer[offset++] = node->hash_range_end;

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

    struct NET_NEW_RANGE_PDU net_new_range = {0};
    net_new_range.type = NET_NEW_RANGE;
    net_new_range.range_start = node->hash_range_start;
    net_new_range.range_end = node->hash_range_end;

    char buffer[sizeof(net_new_range)];
    memcpy(buffer, &net_new_range, sizeof(net_new_range));

    bool send_to_successor = node->hash_range_start == 0;

    int send_status;
    if (send_to_successor) {
        printf("Sending NET_NEW_RANGE message to the successor\n");
        send_status = send(node->sockfd_b, buffer, sizeof(buffer), 0);
        if (send_status == -1) {
            perror("send to successor failed");
            return 1;
        }
        printf("The successor is %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);
    } else {
        printf("Sending NET_NEW_RANGE message to the predecessor\n");
        send_status = send(node->sockfd_d, buffer, sizeof(buffer), 0);
        if (send_status == -1) {
            perror("send to predecessor failed");
            return 1;
        }
        printf("The predecessor is %s:%d\n", inet_ntoa(node->predecessor_ip_address), node->predecessor_port);
    }

    struct NET_NEW_RANGE_RESPONSE_PDU net_new_range_response = {0};
    printf("Waiting for NET_NEW_RANGE_RESPONSE...\n");

    int recv_status;
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

        //printf("The predecessor is %s:%d\n", inet_ntoa(node->predecessor_ip_address), node->predecessor_port);
        int send_status = send(node->sockfd_d, &net_new_range_response, sizeof(net_new_range_response), 0);
        if (send_status == -1) {
            perror("Send failed");
            return 1;
        }
    }

    // Log the updated hash range
    printf("Updated node's range: (%d, %d)\n", node->hash_range_start, node->hash_range_end);

    return 0;
}


// The Q18 state, where we will close the connection with the predecessor and successor and exit gracefully.
int q18_state(void* n, void* data) {
    printf("[q18 state]\n");

    Node* node = (Node*)n;

    bool to_successor = false;
    if (node->hash_range_start == 0) {
        to_successor = true;
    }

    printf("[transfer_all_entries]\n");
    transfer_all_entries(node, to_successor);

    struct NET_CLOSE_CONNECTION_PDU net_close_connection = {0};
    net_close_connection.type = NET_CLOSE_CONNECTION;
    int send_status = send(node->sockfd_b, &net_close_connection, sizeof(net_close_connection), 0);
    if (send_status == -1) {
        perror("send failed");
        return 1;
    }

    struct NET_LEAVING_PDU net_leaving = {0};
    net_leaving.type = NET_LEAVING;
    net_leaving.new_address = node->successor_ip_address.s_addr;
    net_leaving.new_port = htons(node->successor_port);

    int send_leaving_status = send(node->sockfd_d, &net_leaving, sizeof(net_leaving), 0);
    if (send_leaving_status == -1) {
        perror("send failed");
        return 1;
    }

    close_connection(node);
    exit(0);
}
// cargo run --bin client -- --tracker 0.0.0.0:1123 --csv ../../../../TestData/data.csv
// The Q16 state, where the node will receive the NET_LEAVING message from the leaving node.
int q16_state(void* n, void* data) {
    printf("[q16 state]\n");
    Node* node = (Node*)n;
    struct NET_LEAVING_PDU* net_leaving = (struct NET_LEAVING_PDU*)data;

    printf("Received NET_LEAVING message from the leaving node\n");
    printf("Old successor: %s:%d\n", inet_ntoa(node->successor_ip_address), node->successor_port);

    struct in_addr new_successor_addr;
    new_successor_addr.s_addr = net_leaving->new_address;

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &new_successor_addr, ip_str, sizeof(ip_str));
    printf("New successor: %s:%d\n", ip_str, ntohs(net_leaving->new_port));

    // Check if the new successor address and port are valid
    if (new_successor_addr.s_addr == INADDR_NONE || ntohs(net_leaving->new_port) == 0) {
        printf("Invalid successor address or port. Skipping connection attempt.\n");
        return 1;
    }

    if (net_leaving->new_address == node->public_ip.s_addr && ntohs(net_leaving->new_port) == node->port) {
        node->successor_ip_address.s_addr = INADDR_NONE;
        node->successor_port = 0;
        node->sockfd_b = -1;
        printf("Now moving to state 6 as there is no successor.\n");
    } else {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = net_leaving->new_address;
        addr.sin_port = net_leaving->new_port;

        node->sockfd_b = socket(AF_INET, SOCK_STREAM, 0);
        if (node->sockfd_b == -1) {
            perror("socket creation failed");
            return 1;
        }

        int connect_status = connect(node->sockfd_b, (struct sockaddr*)&addr, sizeof(addr));
        if (connect_status == -1) {
            perror("connect failure");
            close(node->sockfd_b);
            node->sockfd_b = -1;
            return 1;
        }

        node->successor_ip_address = new_successor_addr;
        node->successor_port = ntohs(net_leaving->new_port);

        printf("Connected to new successor: %s:%d\n", ip_str, ntohs(net_leaving->new_port));
    }

    printf("Moving to state 6\n");
    node->state_handler = state_handlers[STATE_6];
    node->state_handler(node, NULL);

    return 0;
}