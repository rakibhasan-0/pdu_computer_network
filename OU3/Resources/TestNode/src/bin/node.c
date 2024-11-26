#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../../pdu.h"

/*
Key functionalities:

- Joining the network: A node sends a NET_GET_NODE PDU to the tracker to get an entry point into the network.
  If it is the first node, it initializes the network.

- Maintaining connections: Nodes maintain TCP connections with their predecessor and successor nodes.

- Handling data: Nodes can insert, remove, and lookup data entries. They forward requests to the appropriate node if necessary.

- Alive messages: Nodes periodically send NET_ALIVE messages to the tracker to indicate they are still active.
*/

/*
    Create a Tracker Communication Module:
        Write a basic UDP client to send/receive PDUs to/from the tracker.
        Implement STUN_LOOKUP to get the public address.
        Implement NET_GET_NODE to get information about other nodes.

*/

// typedef the Node structure to Node
typedef struct Node Node;


/*
    the node, shall have a structure that contains information such as  the predecessor, 
    the successor, the hash range, the port number, its public IP address, is alive many other things that I cannot imagine right now.
    The node will be treated as the doubly linked list.
*/

// Node structure, but it may need to change in future. So far soo good.
struct Node{

    struct Node* predecessor;
    struct Node* successor;
    uint16_t hash_range_start;
    uint16_t hash_range_end;
    uint32_t port;
    uint32_t public_ip;
    bool is_alive;

    // tracker's socket address
    struct addrinfo* tracker_addr;
    unsigned int tracker_port;

    // Socket ID for A is used to communicate (sending/reciving) with tracker and outside world
    unsigned int sockfd_a; // UDP connection to tracker, and clinet (most probably)
    // Socket ID for B, is used to communicate(sending/reciving) with the successor.
    unsigned int sockfd_b; // TCP connection to successor
    // Socket ID for C,is used to communicate(sending/reciving) with new conections.
    unsigned int sockfd_c; // TCP connection accepting new connections
    // Socket ID for D is used to communicate(sending/reciving) with the predecessor.
    unsigned int sockfd_d; // TCP connection to predecessor

    // state handler aka function pointer
    int (*state_handler)(Node*);

};


int q1_state(Node* node);
int q2_state(Node* node);
int q3_state(Node* node);
int q4_state(Node* node);

typedef int (*state_handler[])(Node *); // function pointer to manage the states
state_handler state_handlers = {q1_state, q2_state, q3_state, q4_state};


/*
Steps:
-Implement sockets
-STUN_LOOKUP in order to test communication with the tracker
*/
int main(int argc, char* argv[]) {

    int status;
    struct addrinfo hints;
    struct addrinfo *res;


    if(argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        return 1;
    }

    Node* node = (Node*)malloc(sizeof(Node)); // keep it mind other fields allocation and dellocation.
    if(node == NULL) {
        perror("malloc ");
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    status = getaddrinfo(argv[1], argv[2], &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    node->tracker_port = atoi(argv[2]);
    node->tracker_addr = res;
    node->state_handler = state_handlers[0]; // we are initializing the function pointer to point to the q1 state.
    node->state_handler(node); // now we are incoking the function by using the function pointer.


    freeaddrinfo(node->tracker_addr);
    close(node->sockfd_a);
    free(node);

    return 0;
}

// function to handle q1 state
// it creates a socket for the A, send STUN_LOOKUP request to the tracker and move to the next state.
int q1_state(Node *node){

    printf("q1 state\n");
    struct STUN_LOOKUP_PDU stun_lookup;
    memset(&stun_lookup, 0, sizeof(stun_lookup));
    stun_lookup.type = STUN_LOOKUP;

    // now we will create a socket for the A endpoint communication
    node->sockfd_a = socket(node->tracker_addr->ai_family, node->tracker_addr->ai_socktype, node->tracker_addr->ai_protocol);
    if (node->sockfd_a == -1){
        perror("socket faulure");
        return 1;
    }

    // connect the socket to the tracker
    int connect_status = connect(node->sockfd_a, node->tracker_addr->ai_addr, node->tracker_addr->ai_addrlen);
    if (connect_status == -1){
        perror("connect faulure");
        return 1;
    }

    // send Data to the tracker
    int send_status = sendto(node->sockfd_a, &stun_lookup, sizeof(stun_lookup), 0, node->tracker_addr->ai_addr,
                             node->tracker_addr->ai_addrlen);
    if (send_status == -1){
        perror("send faulure");
        return 1;
    }

    // move to the next state q2
    node->state_handler = state_handlers[1];
    node->state_handler(node);
}

// function to handle q2 state
// that following function will recieve respons from the tracker, based on the information it
// will move to the next state.
int q2_state(Node *node){

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

    // convert the address to the network byte order
    if (stun_response.type == STUN_RESPONSE){
        struct in_addr addr = {stun_response.address};
        printf("Public IP: %s\n", inet_ntoa(addr));
    }

    // move to the next state q3
    node->state_handler = state_handlers[2];
    node->state_handler(node);
}

int q3_state(Node *node){
    printf("q3 state\n");
    // we will send NET_GET_NODE to the tracker
    struct NET_GET_NODE_PDU net_get_node = {0};
    net_get_node.type = NET_GET_NODE;

    // it is time to send the NET_GET_NODE to the tracker via UDP
    int send_status = sendto(node->sockfd_a, &net_get_node, sizeof(net_get_node), 0,
                             node->tracker_addr->ai_addr, node->tracker_addr->ai_addrlen);

    if (send_status == -1){
        perror("send faulure");
        return 1;
    }

    // net_get_node_response will store the response from the tracker
    struct NET_GET_NODE_RESPONSE_PDU net_get_node_response = {0};
    // we will create a buffer to store the response from the tracker
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    // now we will try to recieve the response from the tracker
    int rcv_data = recvfrom(node->sockfd_a, buffer, sizeof(buffer), 0, node->tracker_addr->ai_addr,
                            &node->tracker_addr->ai_addrlen);

    // removing the padding from the buffer and storing it in the net_get_node_response
    memcpy(&net_get_node_response.type, buffer, sizeof(net_get_node_response.type));
    memcpy(&net_get_node_response.address, buffer + sizeof(net_get_node_response.type), sizeof(net_get_node_response.address));
    memcpy(&net_get_node_response.port, buffer + sizeof(net_get_node_response.type) + sizeof(net_get_node_response.address),
           sizeof(net_get_node_response.port));

    // check if the response is NET_GET_NODE_RESPONSE is empty
    if (net_get_node_response.type == NET_GET_NODE_RESPONSE){
        struct in_addr addr;
        addr.s_addr = net_get_node_response.address;
        printf("Node IP: %s\n", inet_ntoa(addr));
        printf("Node Port: %d\n", ntohs(net_get_node_response.port));
        if (net_get_node_response.address == 0 && net_get_node_response.port == 0){
            printf("Empty response\n");
            printf("nothing to do right now\n");
        }
    }

    // move to the next state q4
    node->state_handler = state_handlers[3];
    node->state_handler(node);
}

// state number 4, we dont know anything about this state yet.
int q4_state(Node *node){
    printf("q4 state\n");
    return 0;
}