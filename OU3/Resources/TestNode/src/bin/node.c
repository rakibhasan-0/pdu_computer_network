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
#include <poll.h>
#include <time.h>
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

    struct in_addr predecessor_ip_address;
    uint16_t predecessor_port;

    struct in_addr successor_ip_address;
    uint16_t successor_port;

    uint8_t hash_range_start; // hash will range in between 0 to 255.
    uint8_t hash_range_end;
    uint8_t hash_span; 

    uint16_t port;
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
int q12_state(void* n, void* data);
int q8_state(void* n, void* data);
uint8_t calulate_hash_span(uint8_t start, uint8_t end);

typedef int (*state_handler[])(void*, void*); // function pointer to manage the states


state_handler state_handlers = {q1_state, q2_state, q3_state, q4_state, q5_state, q6_state, q7_state, q12_state, q8_state};


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

    node->port = ntohs(addr.sin_port);

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

}


// based on the graph, I(gazi) think that state_6 kinda core since most states are being handled from here.
int q6_state(void* n, void* data){

    Node* node = (Node*)n;
    printf("q6 state\n");
    struct NET_ALIVE_PDU net_alive = {0};
    net_alive.type = NET_ALIVE;
   
    node->is_alive = true;

    if(node->sockfd_c == 0){

        node->sockfd_c = socket(AF_INET, SOCK_STREAM, 0);
        if (node->sockfd_c == -1) {
            perror("socket failure");
            return 1;
        }

        struct sockaddr_in addr_sock_c;
        memset(&addr_sock_c, 0, sizeof(addr_sock_c));
        addr_sock_c.sin_family = AF_INET;
        addr_sock_c.sin_addr.s_addr = node->public_ip.s_addr;
        addr_sock_c.sin_port = node->port;

        int bind_status = bind(node->sockfd_c, (struct sockaddr*)&addr_sock_c, sizeof(addr_sock_c));
        if (bind_status == -1) {
            printf("bind failure __happened here\n");
            perror("bind failure");
            return 1;
        }

        int listen_status = listen(node->sockfd_c, 1);
        if (listen_status == -1) {
            perror("listen failure");
            return 1;
        }
    }
    
    // time interval for the alive message
    int timeout = 1;
    time_t last_time = time(NULL);
    struct pollfd poll_fd [2];
    poll_fd[0].fd = node->sockfd_a;
    poll_fd[0].events = POLLIN;
    poll_fd[1].fd = node->sockfd_c;
    poll_fd[1].events = POLLIN;


    while(1){

        printf("we are in the while loop of the q6 state\n");

        time_t current_time = time(NULL);

        if (current_time - last_time >= timeout){
            int send_status = sendto(node->sockfd_a, &net_alive, sizeof(net_alive), 0,
                                     node->tracker_addr->ai_addr, node->tracker_addr->ai_addrlen);
            last_time = current_time;
            timeout = 6;
        }

        int poll_status = poll(poll_fd, 2, 500); 
        if (poll_status == -1){
            perror("poll failure");
            return 1;
        }else if (poll_status == 0){
            continue;
        }

        for(int i = 0; i < 2; i++){

            if(poll_fd[i].fd == node->sockfd_a){
                struct sockaddr_in sender_addr;
                socklen_t sender_addr_len = sizeof(sender_addr);
                struct NET_JOIN_PDU net_join = {0};

                
                int rcv_data = recvfrom(node->sockfd_a, &net_join, sizeof(net_join), 0, (struct sockaddr *)&sender_addr, &sender_addr_len);
                if (rcv_data == -1) {
                    perror("recvfrom failed");
                    continue;
                }

                // we are adding the sender port to the net_join
                net_join.src_port = ntohs(sender_addr.sin_port);

                printf("Received NET_JOIN (Q6)\n");
                printf("Type (Q6): %d\n", net_join.type);
                printf("Source Address(Q6): %s\n", inet_ntoa((struct in_addr){.s_addr = net_join.src_address}));
                printf("Source Port(Q6): %d\n", net_join.src_port);
                printf("Max address(Q6): %s\n", inet_ntoa((struct in_addr){.s_addr = net_join.max_address}));
                printf("Max Port(Q6): %d\n", net_join.max_port);
                printf("we are abour to move to the q12 state from q6\n");
                node->state_handler = state_handlers[7];
                node->state_handler(node, &net_join);

            }

            if(poll_fd[i].fd == node->sockfd_c){
                struct sockaddr_in sender_addr;
                socklen_t sender_addr_len = sizeof(sender_addr);

                int accept_status = accept(node->sockfd_c, (struct sockaddr*)&sender_addr, &sender_addr_len);

                if(poll_fd[i].revents == POLLIN){
                    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};
                    int recv_status = recv(accept_status, &net_join_response , sizeof(net_join_response), 0);

                    printf("Received NET_JOIN_RESPONSE\n");
                    printf("the number of bytes received: %d\n", recv_status);
                    printf("Next Address: %s\n", inet_ntoa((struct in_addr){.s_addr = net_join_response.next_address}));
                    printf("Next Port: %d\n", ntohs(net_join_response.next_port));
                    printf("Range Start: %d\n", net_join_response.range_start);
                    printf("Range End: %d\n", net_join_response.range_end);
                    printf("we shall not exit the q6 state\n");

                }
                close(accept_status);
            
            }

        }

    }

}






// that state will NET_JOIN to the node that we got from the tracker.
// what I think i will start traversing the networ from the node that we got from the tracker.
// we will make the node the predecessor the founded node of the node.
int q7_state(void* n, void* data) {
    printf("q7 state\n");

    struct NET_GET_NODE_RESPONSE_PDU* net_get_node_response = (struct NET_GET_NODE_RESPONSE_PDU*)data;
    Node* node = (Node*)n;
    
    printf("max port %d\n", ntohs(net_get_node_response->port));
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

    ssize_t bytes_sent = sendto(node->sockfd_a, &net_join, sizeof(net_join), 0, (struct sockaddr *)&addr, sizeof(addr));
    if (bytes_sent == -1) {
        perror("sendto failed");
        return 1;
    }

    // we will listen to the NET_JOIN_RESPONSE from the node
    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};
    struct sockaddr_in sender_addr;
    
    node->sockfd_c = socket(AF_INET, SOCK_STREAM, 0);
    if (node->sockfd_b == -1) {
        perror("socket failed");
        return 1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = node->public_ip.s_addr;
    addr.sin_port = htons(node->port);

    int bind_status = bind(node->sockfd_c, (struct sockaddr *)&addr, sizeof(addr));
    if (bind_status == -1) {
        perror("bind failed");
        return 1;
    }
    int listen_status = listen(node->sockfd_c, 1);
    if (listen_status == -1) {
        perror("listen failed");
        return 1;
    }

    socklen_t sender_addr_len = sizeof(sender_addr); 
    int accept_status = accept(node->sockfd_c, (struct sockaddr*)&sender_addr, &sender_addr_len);
    if (accept_status == -1) {
        perror("accept failed");
        return 1;
    }

    int recv_status = recv(accept_status, &net_join_response, sizeof(net_join_response), 0);
    printf("Received NET_JOIN_RESPONSE\n");
    printf("The number of bytes received: %d\n", recv_status);
    printf("Next Address: %s\n", inet_ntoa((struct in_addr){.s_addr = ntohl(net_join_response.next_address)}));
    printf("Next Port: %d\n", ntohs(net_join_response.next_port)); 
    printf("Range Start: %d\n", net_join_response.range_start);
    printf("Range End: %d\n", net_join_response.range_end);

    // accept predecessor means the src address of the net_join_response is the predecessor of the node.
    node->predecessor_ip_address.s_addr = sender_addr.sin_addr.s_addr;
    node->predecessor_port = sender_addr.sin_port;
    node->hash_range_start = net_join_response.range_start;
    node->hash_range_end = net_join_response.range_end;
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);
    


    // we will move to the q8 state and connect to the successor.
    node->state_handler = state_handlers[8];
    node->state_handler(node, &net_join_response);


    return 0;
}


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
    net_join->max_port = htons(node->port);
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
    }

    return 0;
}


int q5_state(void* n, void* data) {
    printf("q5 state\n");

    Node* node = (Node*)n;
    struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)data;

    node->predecessor_ip_address.s_addr = net_join->src_address; 
    node->predecessor_port = net_join->src_port;

    node->successor_ip_address.s_addr = net_join->src_address; 
    node->successor_port = net_join->src_port;
    node->hash_range_start = 0;  
    node->hash_range_end = 127;
    node->hash_span = calulate_hash_span(node->hash_range_start, node->hash_range_end);

    printf("Predecessor IP: %s\n", inet_ntoa(node->predecessor_ip_address));
    printf("Predecessor Port: %d\n", node->predecessor_port);
    printf("Successor IP: %s\n", inet_ntoa(node->successor_ip_address));
    printf("Successor Port: %d\n", node->successor_port);
    printf("Hash Span: %d\n", node->hash_span);
    printf("Hash Range Start: %d\n", node->hash_range_start);
    printf("Hash Range End: %d\n", node->hash_range_end);

    struct NET_JOIN_RESPONSE_PDU net_join_response = {0};
    net_join_response.type = NET_JOIN_RESPONSE;
    net_join_response.next_address = htonl(node->public_ip.s_addr);
    net_join_response.next_port = htons(node->port);
    net_join_response.range_start = 128;  // just for the testing purpose
    net_join_response.range_end = 255;


    node->sockfd_c = socket(AF_INET, SOCK_STREAM, 0);
    if (node->sockfd_c == -1) {
        perror("socket failure");
        return 1;
    }

    printf("Node IP Address: %s\n", inet_ntoa(node->public_ip));
    printf("Node Port: %d\n", node->port);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = net_join->src_address;
    addr.sin_port = htons(net_join->src_port);  

    // Connect to the sender
    int connect_status = connect(node->sockfd_c, (struct sockaddr*)&addr, sizeof(addr));
    if (connect_status == -1) {
        perror("connect failure");
        return 1;
    }

    // Send the NET_JOIN_RESPONSE
    int send_status = send(node->sockfd_c, &net_join_response, sizeof(net_join_response), 0);
    if (send_status == -1) {
        perror("send failure");
        return 1;
    }

    // we may close the connect of the socket_c here.
    close(node->sockfd_c);
    // Return to q6 state
    node->state_handler = state_handlers[5];
    node->state_handler(node, NULL);


    return 0;
}


int q8_state(void* n, void* data){

    printf("q8 state\n");
    Node* node = (Node*)n;
    struct NET_JOIN_RESPONSE_PDU* net_join_response = (struct NET_JOIN_RESPONSE_PDU*)data;
    node->successor_ip_address.s_addr = ntohl(net_join_response->next_address);
    node->successor_port = ntohs(net_join_response->next_port);

    printf("Predecessor IP: %s\n", inet_ntoa(node->predecessor_ip_address));
    printf("Predecessor Port: %d\n", node->predecessor_port);
    printf("Hash Span: %d\n", node->hash_span);
    printf("Hash Range Start: %d\n", node->hash_range_start);
    printf("Hash Range End: %d\n", node->hash_range_end);
    printf("node's own public ip: %s\n", inet_ntoa(node->public_ip));
    printf("node's own port: %d\n", node->port);
    printf("node's successor ip: %s\n", inet_ntoa(node->successor_ip_address));
    printf("node's successor port: %d\n", node->successor_port);

    //we will move to the q6 state
    node->state_handler = state_handlers[5];
    node->state_handler(node, NULL);


}


uint8_t calulate_hash_span(uint8_t start, uint8_t end){
    printf("start:--from calc hash_span %d\n", start);
    printf("end:--from clac hash_span %d\n", end);

    return (end - start + 1) % 256;
}



