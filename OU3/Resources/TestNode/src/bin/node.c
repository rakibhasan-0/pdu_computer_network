#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <pdu.h>

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

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    status = getaddrinfo(argv[1], argv[2], &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sockfd == -1) {
        perror("socket");
        return 1;
    }

    // send a STUN_LOOKUP PDU to the tracker


    return 0;
}