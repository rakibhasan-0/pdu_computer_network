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

    struct Node* predecessor; // we dont need the port of the predecessor, because we can connect the port by through the node and its pointer.
    struct Node* successor;
    uint8_t hash_range_start; // hash will range in between 0 to 255.
    uint8_t hash_range_end;
    uint32_t port;
    struct in_addr public_ip;
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
    int (*state_handler)(void*, void*);

};

// the reason we have choosen two function parameters so that we have the flexibility to pass the data between the states.
int q1_state(void* n, void* data);
int q2_state(void* n, void* data);
int q3_state(void* n, void* data);
int q4_state(void* n, void* data);
int q6_state(void* n, void* data);
int q7_state(void* n, void* data);
int q5_state(void* n, void* data);

typedef int (*state_handler[])(void*, void*); // function pointer to manage the states
state_handler state_handlers = {q1_state, q2_state, q3_state, q4_state, q5_state, q6_state, q7_state};


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
    node->state_handler(node, NULL); // now we are incoking the function by using the function pointer.


    freeaddrinfo(node->tracker_addr);
    close(node->sockfd_a);
    free(node);

    return 0;
}

// function to handle q1 state
// it creates a socket for the A, send STUN_LOOKUP request to the tracker and move to the next state.
int q1_state(void* n, void* data){

    Node* node = (Node*)n;

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

    // send Data to the tracker
    int send_status = sendto(node->sockfd_a, &stun_lookup, sizeof(stun_lookup), 0, node->tracker_addr->ai_addr,
                             node->tracker_addr->ai_addrlen);
    if (send_status == -1){
        perror("send faulure");
        return 1;
    }

    // move to the next state q2
    node->state_handler = state_handlers[1];
    node->state_handler(node, NULL);
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

    // convert the address to the network byte order
    if (stun_response.type == STUN_RESPONSE){
        struct in_addr addr = {stun_response.address};
        node->public_ip = addr; // we are storing the public ip address of the node.
        printf("Public IP: %s\n", inet_ntoa(addr));
    }


    // move to the next state q3
    node->state_handler = state_handlers[2];
    node->state_handler(node, NULL);
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
        printf("Node IP: %s\n", inet_ntoa(addr));
        printf("Node Port: %d\n", ntohs(net_get_node_response->port));
        if (net_get_node_response->address == 0 && net_get_node_response->port == 0){
            printf("Empty response from the Tracker\n");
            // move to the state 4
            node-> state_handler = state_handlers[3];
            node->state_handler(node, NULL);
        }else{
            // we will move to the state q7, since the respons is not empty.
            // what I guess that net_get_node_response will contain the information about a node in the network.
            // it will contain the address and the port of the node.
            // the main task of the 
            node->state_handler = state_handlers[6]; // now we are going to the q7 state.
            node->state_handler(node, net_get_node_response);
        }

        
    }
}

// state number 4, we dont know anything about this state yet.
// 
int q4_state(void* n, void* data){

    Node* node = (Node*)n;
    printf("q4 state\n");
    // the node itself will be its predecessor and successor
    // the initialization of the 
    node->predecessor = node;
    node->successor = node;
    node->hash_range_start = 0;
    node->hash_range_end = 255; // since there is only one node. 

    // start sending alive messages to the tracker
    node->state_handler = state_handlers[5]; // it will move to the q6 state
    node->state_handler(node, NULL);

}


// based on the graph, I(gazi) think that state_6 kinda core since most states are being handled from here.
int q6_state(void* n, void* data){

    Node* node = (Node*)n;
    printf("q6 state\n");
    struct NET_ALIVE_PDU net_alive = {0};
    net_alive.type = NET_ALIVE;
   
    node->is_alive = true;
    // we are binding the socekt_a to the node's public ip address and port.
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = node->public_ip.s_addr;
    addr.sin_port = htons(node->port);
    printf("Binding to IP: %s, Port: %d\n",
       inet_ntoa(node->public_ip), node->port);


    while(1){
        sleep(7);
        int send_status = sendto(node->sockfd_a, &net_alive, sizeof(net_alive), 0,
                                 node->tracker_addr->ai_addr, node->tracker_addr->ai_addrlen);
        
        // now we will receive response from the new node that contains the information about the NET_JOIN
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        struct sockaddr_in sender_addr;
        socklen_t sender_addr_len = sizeof(sender_addr);
        memset(&sender_addr, 0, sizeof(sender_addr));

        int rcv_data = recvfrom(node->sockfd_a, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender_addr, &sender_addr_len);

        // the buffer shall recive the new node's information.
        // deserialize the buffer and store it in the NET_JOIN_PDU
        struct NET_JOIN_PDU net_join = {0};
        memcpy(&net_join.type, buffer, sizeof(net_join.type));
        memcpy(&net_join.src_address, buffer + sizeof(net_join.type), sizeof(net_join.src_address));
        memcpy(&net_join.src_port, buffer + sizeof(net_join.type) + sizeof(net_join.src_address), sizeof(net_join.src_port));
        memcpy(&net_join.max_span, buffer + sizeof(net_join.type) + sizeof(net_join.src_address) + sizeof(net_join.src_port), sizeof(net_join.max_span));
        memcpy(&net_join.max_address, buffer + sizeof(net_join.type) + sizeof(net_join.src_address) + sizeof(net_join.src_port) + sizeof(net_join.max_span), sizeof(net_join.max_address));
        memcpy(&net_join.max_port, buffer + sizeof(net_join.type) + sizeof(net_join.src_address) + sizeof(net_join.src_port) + sizeof(net_join.max_span) + sizeof(net_join.max_address), sizeof(net_join.max_port));

        printf("New Node IP: %s\n", inet_ntoa(sender_addr.sin_addr));
        printf("New Node Port: %d\n", ntohs(sender_addr.sin_port));



        if (rcv_data == -1){
            perror("recv faulure as the address is not known lol");
            return 1;
        }


        printf("Alive message sent to the tracker \n");

    }


}


// that state will NET_JOIN to the node that we got from the tracker.
// what I think i will start traversing the networ from the node that we got from the tracker.
// we will make the node the predecessor the founded node of the node.
int q7_state(void* n, void* data){
    printf("q7 state\n");
    Node* node = (Node*)n;
	struct NET_GET_NODE_RESPONSE_PDU net_get_node_response;
	
	//Skaffa successor infon

	// Create socket for communicating with the active node via TCP
	node->sockfd_b = socket();
    if (node->sockfd_b == -1) {
        perror("socket failure");
        return 1;
    }

	int connect_status = connect(node->sockfd_b);
    if (connect_status == -1) {
        perror("connect failure");
        return 1;
    }

	//NET_JOIN



    // we get respons from the tracker in the get_node_response it contains the address, which is in the network.
    // but the question is how the node in network recieves the messge from the new node via udp
    // it does not know about the new nodes address and port before recieving the message.
    struct NET_GET_NODE_RESPONSE_PDU* net_get_node_response = (struct NET_GET_NODE_RESPONSE_PDU*)data;
    Node* node = (Node*)n;

    struct NET_JOIN_PDU net_join = {0};
    net_join.type = NET_JOIN;
    net_join.src_address = node->public_ip.s_addr;
    net_join.src_port = node->port;
    net_join.max_span = 0;
    net_join.max_address = 0;
    net_join.max_port = 0;

    // now we will send the NET_JOIN to the the node that we got from the tracker.
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = net_get_node_response->address; 
    addr.sin_port = net_get_node_response->port;
    int send_status = sendto(node->sockfd_a, &net_join, sizeof(net_join), 0, 
                            (struct sockaddr*)&addr, sizeof(addr));


    // now new will wait for the response from its predecessor.
    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    /// precdecessor will send the response to the new node, but it creates a new socket, it establishes a new TCP connection.


    


}


int q5_state(void* n, void* data){
    printf("q5 state\n");
    printf("nothing to do here\n");
    return 0;

} 

