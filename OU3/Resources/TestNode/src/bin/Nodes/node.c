#include "node.h"


state_handler state_handlers[] = {
    q1_state,   // State 0
    q2_state,   // State 1
    q3_state,   // State 2
    q4_state,   // State 3
    q5_state,   // State 4
    q6_state,   // State 5
    q7_state,   // State 6
    q12_state,  // State 7
    q8_state    // State 8
};

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



