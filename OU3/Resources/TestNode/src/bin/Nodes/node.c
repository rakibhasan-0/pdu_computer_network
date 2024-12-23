#include "node.h"


state_handler state_handlers[] = {
    q1_state, // 0
    q2_state, // 1 
    q3_state, // 2
    q4_state, // 3
    q5_state,  // 4
    q6_state,   // 5
    q7_state,  // 6
    q12_state, // 7
    q8_state,  // 8
    q13_state, // 9
    q14_state, // 10
    q17_state,   // 11
    q10_state, // 12
    q11_state, // 13
    q15_state, // 14
    q18_state, // 15
    q16_state, // 16
    q9_state, // 17
};

/*
Steps:
-Implement sockets
-STUN_LOOKUP in order to test communication with the tracker
*/

volatile sig_atomic_t should_close = 0; // creaeting should_close as a global variable, initially it is set to 0 as false.

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

    node->hash_table = ht_create(NULL); // we are creating the hash table.
    register_signal_handlers(); // we are about to register the signal handlers.
    node->queue_for_values = queue_create(10); // we are creating the queue for the values.

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    status = getaddrinfo(argv[1], argv[2], &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }


    printf("Node started and it is sending STUN_LOOKUP to the tracker %s:%d\n", argv[1], atoi(argv[2]));

    node->tracker_port = atoi(argv[2]);
    node->tracker_addr = res;
    node->state_handler = state_handlers[0]; // we are initializing the function pointer to point to the q1 state.
    node->state_handler(node, NULL); // now we are incoking the function by using the function pointer.



    freeaddrinfo(node->tracker_addr);
    close(node->sockfd_a);
    free(node);

    return 0;
}



