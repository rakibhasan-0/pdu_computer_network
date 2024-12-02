#include "initialization.h"


// function to handle q1 state
// it creates a socket for the A, send STUN_LOOKUP request to the tracker and move to the next state.
int q1_state(void* n, void* data) {
    Node* node = (Node*)n;

    printf("q1 state\n");
    struct STUN_LOOKUP_PDU stun_lookup;
    memset(&stun_lookup, 0, sizeof(stun_lookup));
    stun_lookup.type = STUN_LOOKUP;

    node->sockfd_a = socket(node->tracker_addr->ai_family, node->tracker_addr->ai_socktype, node->tracker_addr->ai_protocol);
    if (node->sockfd_a == -1) {
        perror("socket failure");
        return 1;
    }

    // we are craeting a socket for the listener
    node->listener_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (node->listener_socket == -1) {
        perror("socket failure");
        return 1;
    }


    struct sockaddr_in addr_sock_c;
    memset(&addr_sock_c, 0, sizeof(addr_sock_c));
    addr_sock_c.sin_family = AF_INET;
    addr_sock_c.sin_addr.s_addr = node->public_ip.s_addr;


    int bind_status = bind(node->listener_socket, (struct sockaddr*)&addr_sock_c, sizeof(addr_sock_c));
    if (bind_status == -1) {
        printf("bind failure __happened here\n");
        perror("bind failure");
        return 1;
    }

    int listen_status = listen(node->listener_socket, 1);
    if (listen_status == -1) {
        perror("listen failure");
        return 1;
    }

     // get the port number of the listener socket
    struct sockaddr_in addr_listener;
    socklen_t addr_len_listener = sizeof(addr_listener);
    if (getsockname(node->listener_socket, (struct sockaddr*)&addr_listener, &addr_len_listener) == -1) {
        perror("getsockname failed");
        return 1;
    }

    node->port = ntohs(addr_listener.sin_port);
    printf("Listener Port: %d\n", node->port);

   
    int send_status = sendto(node->sockfd_a, &stun_lookup, sizeof(stun_lookup), 0, node->tracker_addr->ai_addr,
                             node->tracker_addr->ai_addrlen);
    if (send_status == -1) {
        perror("send failure");
        return 1;
    }

    // trying to retrieve the port number of the socket_a
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(node->sockfd_a, (struct sockaddr*)&addr, &addr_len) == -1) {
        perror("getsockname failed");
        return 1;
    }

    // we are now moving to the next state q2
    node->state_handler = state_handlers[1];
    node->state_handler(node, NULL);

    return 0;
}

// function to handle q2 state
// that following function will recieve respons from the tracker, based on the information it
// will move to the next state.
int q2_state(void* n, void* data){

    Node* node = (Node*)n;
    // we will create a buffer to store the response from the tracker
    printf("q2 state\n");
    char buffer[1024];
    // nullify the buffer
    memset(buffer, 0, sizeof(buffer));
    struct STUN_RESPONSE_PDU stun_response = {0};
    stun_response.type = STUN_RESPONSE;

    // now we will try to recieve the response from the tracker
    int rcv_data = recvfrom(node->sockfd_a, buffer, sizeof(buffer), 0, node->tracker_addr->ai_addr,
                            &node->tracker_addr->ai_addrlen);
    if (rcv_data == -1){
        perror("recv faulure");
        return 1;
    }

    // now remove the padding from the buffer and store it in the stun_response
    memcpy(&stun_response.type, buffer, sizeof(stun_response.type));
    memcpy(&stun_response.address, buffer + sizeof(stun_response.type), sizeof(stun_response.address));

    
    if (stun_response.type == STUN_RESPONSE){
        struct in_addr addr_pub = {stun_response.address};
        node->public_ip = addr_pub; // we are storing the public ip address of the node.
        printf("the node's  Port: %d\n", node->port);
        printf("Public IP: %s\n", inet_ntoa(addr_pub));
    }


    // move to the next state q3
    node->state_handler = state_handlers[2];
    node->state_handler(node, NULL);

    return 0;
}


int q3_state(void* n, void* data){

    Node* node = (Node*)n;
    printf("q3 state\n");
    // we will send NET_GET_NODE to the tracker
    struct NET_GET_NODE_PDU net_get_node = {0};
    // just for check if the public ip is stored in the node
    //printf("Public IP from state 3: %s\n", inet_ntoa(node->public_ip));

    net_get_node.type = NET_GET_NODE;

    // it is time to send the NET_GET_NODE to the tracker via UDP
    int send_status = sendto(node->sockfd_a, &net_get_node, sizeof(net_get_node), 0,
                             node->tracker_addr->ai_addr, node->tracker_addr->ai_addrlen);

    if (send_status == -1){
        perror("send faulure");
        return 1;
    }

    // net_get_node_response will store the response from the tracker
    struct NET_GET_NODE_RESPONSE_PDU* net_get_node_response = calloc(1, sizeof(struct NET_GET_NODE_RESPONSE_PDU)); // as we are senfing to the other function.
    // we will create a buffer to store the response from the tracker
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    // now we will try to recieve the response from the tracker
    int rcv_data = recvfrom(node->sockfd_a, buffer, sizeof(buffer), 0, node->tracker_addr->ai_addr,
                            &node->tracker_addr->ai_addrlen);

    // removing the padding from the buffer and storing it in the net_get_node_response
    memcpy(&net_get_node_response->type, buffer, sizeof(net_get_node_response->type));
    memcpy(&net_get_node_response->address, buffer + sizeof(net_get_node_response->type), sizeof(net_get_node_response->address));
    memcpy(&net_get_node_response->port, buffer + sizeof(net_get_node_response->type) + sizeof(net_get_node_response->address),
           sizeof(net_get_node_response->port));

    // check if the response is NET_GET_NODE_RESPONSE is empty
    if (net_get_node_response->type == NET_GET_NODE_RESPONSE){
        struct in_addr addr;
        addr.s_addr = net_get_node_response->address;
        

        printf("net get response aka entry node's IP: %s\n", inet_ntoa(addr));
        printf("net get response aka entry node's node Port: %d\n", ntohs(net_get_node_response->port));
    


        if (net_get_node_response->address == 0 && net_get_node_response->port == 0){
            printf("Empty response from the Tracker\n");
            // move to the state 4
            printf(" we can here\n");
            printf("we are about to move to the q4 state from q3\n");
            node-> state_handler = state_handlers[3];
            node->state_handler(node, NULL);
        }else{
            // we will move to the state q7, since the respons is not empty.
            // what I guess that net_get_node_response will contain the information about a node in the network.
            // it will contain the address and the port of the node.
            // the main task of the 
            node->state_handler = state_handlers[6]; // now we are going to the q7 state.
            printf("we are about to move q7 state from q3\n");
            node->state_handler(node, net_get_node_response);
        }

        
    }

    return 0;
}
