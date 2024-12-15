#ifndef NODE_H
#define NODE_H

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
#include <fcntl.h> 
#include <math.h>
#include <signal.h>
#include <errno.h>
#include "../../../../pdu.h"

#include "initialization.h"
#include "join_network.h"
#include "controller.h"
#include "communication.h"
#include "signal_handler.h"
#include "utils.h"

#define STATE_13 9
#define STATE_14 10
#define STATE_17 11
#define STATE_6 5
#define STATE_5 4
#define STATE_12 7
#define STATE_8 8
#define STATE_10 12
#define STATE_11 13
#define STATE_15 14
#define STATE_18 15
#define STATE_16 16
#define STATE_9 17

typedef struct Node Node;

// Node structure
struct Node {
    // Predecessor and successor details
    struct in_addr predecessor_ip_address;
    uint16_t predecessor_port;
    struct in_addr successor_ip_address;
    uint16_t successor_port;

    // Hash range and span for routing or hashing
    uint8_t hash_range_start;  // Range from 0 to 255
    uint8_t hash_range_end;
    uint8_t hash_span;

    // Node's local details
    uint16_t port;
    struct in_addr public_ip;
    bool is_alive;

    // Tracker's socket information
    struct addrinfo* tracker_addr;   // Tracker's address info
    unsigned int tracker_port;        // Tracker's port

    // Socket IDs for various communications
    int sockfd_a; // UDP connection to tracker/client
    int sockfd_b; // TCP connection to successor
    int listener_socket;  // TCP listening socket for new connections
    int sockfd_d;         // TCP connection to predecessor

    // State handler function pointer (for state transitions)
    int (*state_handler)(void*, void*);
};

// Function pointer for state handlers
typedef int (*state_handler)(void *node, void *data);
extern state_handler state_handlers[];

// Global variable to indicate if the program should close
extern volatile sig_atomic_t should_close;

#endif // NODE_H
