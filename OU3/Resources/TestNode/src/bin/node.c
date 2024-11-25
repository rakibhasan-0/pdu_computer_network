#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
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
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    status = getaddrinfo(argv[1], argv[2], &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket ");
        return 1;
    }

    // send a STUN_LOOKUP PDU to the tracker
    struct STUN_LOOKUP_PDU stun_lookup;
    stun_lookup.type = STUN_LOOKUP;
    int send_status = sendto(sockfd, &stun_lookup, sizeof(stun_lookup), 0, res->ai_addr, res->ai_addrlen);
    if(send_status == -1) {
        perror("it failed to send the STUN_LOOKUP PDU");
        return 1;
    }

    // receive a STUN_RESPONSE PDU from the tracker
    struct STUN_RESPONSE_PDU stun_response;
    int rcv_status = recvfrom(sockfd, &stun_response, sizeof(stun_response), 0, res->ai_addr, &res->ai_addrlen);
    if(rcv_status == -1) {
        perror("it failed to receive the STUN_RESPONSE PDU");
        return 1;
    }

    
    if (stun_response.type == STUN_RESPONSE) {
        struct in_addr addr = {stun_response.address};
        printf("Public IP: %s\n", inet_ntoa(addr));
    }


    // it is time to node send NET_GET_NODE to the tracker
    struct NET_GET_NODE_PDU net_get_node;
    net_get_node.type = NET_GET_NODE;
    
    send_status = sendto(sockfd, &net_get_node, sizeof(net_get_node), 0, res->ai_addr, res->ai_addrlen);
    if(send_status == -1) {
        perror("it failed to send the NET_GET_NODE PDU");
        return 1;
    }

    struct NET_GET_NODE_RESPONSE_PDU net_get_node_response;
    rcv_status = recvfrom(sockfd, &net_get_node_response, sizeof(net_get_node_response), 0, res->ai_addr, &res->ai_addrlen);
    if(rcv_status == -1) {
        perror("it failed to receive the NET_GET_NODE_RESPONSE PDU");
        return 1;
    }


    if(net_get_node_response.type == NET_GET_NODE_RESPONSE) {
        struct in_addr addr;
        addr.s_addr = net_get_node_response.address;
        printf("Node IP: %s\n", inet_ntoa(addr));
        printf("Node Port: %d\n", ntohs(net_get_node_response.port));
    }


    while(1){
        if(net_get_node_response.address == 0 && net_get_node_response.port == 0) {
            printf("The network is empty\n");
        }
    }

    
    // since it is empty, we can sent 

    freeaddrinfo(res);
    close(sockfd);


    return 0;
}