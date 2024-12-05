#include "node.h"


state_handler state_handlers[] = {
    q1_state,
    q2_state,
    q3_state,
    q4_state, 
    q5_state,  
    q6_state,   
    q7_state,  
    q12_state, 
    q8_state,   
	q9_state
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

    // to make ip address format
   

    printf("Node started and it is sending STUN_LOOKUP to the tracker %d, %d\n", argv[1], atoi(argv[2]));
    node->tracker_port = atoi(argv[2]);
    node->tracker_addr = res;
    node->state_handler = state_handlers[0]; // we are initializing the function pointer to point to the q1 state.
    node->state_handler(node, NULL); // now we are incoking the function by using the function pointer.



    freeaddrinfo(node->tracker_addr);
    close(node->sockfd_a);
    free(node);

    return 0;
}



