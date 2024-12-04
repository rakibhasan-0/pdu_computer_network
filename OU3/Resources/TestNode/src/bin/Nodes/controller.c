#include "controller.h"

// based on the graph, I(gazi) think that state_6 kinda core since most states are being handled from here.
int q6_state(void* n, void* data){

    Node* node = (Node*)n;
    printf("[q6 state]\n");

	/* For testing Q9
    if () {
        node->state_handler = state_handlers[9]; // Q9 state
        node->state_handler(node, NULL);
        return 0;
    }
	*/

    struct NET_ALIVE_PDU net_alive = {0};
    net_alive.type = NET_ALIVE;
   
    node->is_alive = true;
    
    // time interval for the alive message
    int timeout = 1;
    time_t last_time = time(NULL);
    struct pollfd poll_fd[4];
    poll_fd[0].fd = node->sockfd_a;
    poll_fd[0].events = POLLIN;
    poll_fd[1].fd = node->listener_socket;
    poll_fd[1].events = POLLIN;
    poll_fd[2].fd = node->sockfd_b;
    poll_fd[2].events = POLLIN;
    poll_fd[3].fd = node->sockfd_d;
    poll_fd[3].events = POLLIN;


    while(1){
        time_t current_time = time(NULL);

        if (current_time - last_time >= timeout){
            int send_status = sendto(node->sockfd_a, &net_alive, sizeof(net_alive), 0,
                                     node->tracker_addr->ai_addr, node->tracker_addr->ai_addrlen);
            last_time = current_time;
            timeout = 7;
        }

        int poll_status = poll(poll_fd, 4, 500); 
        if (poll_status == -1){
            perror("poll failure");
            return 1;
        }else if (poll_status == 0){
            continue;
        }

        for(int i = 0; i < 4; i++){

            if(poll_fd[i].revents == POLLIN){

               //printf("Received something\n");

                if(poll_fd[i].fd == node->sockfd_a){
                  
                    struct sockaddr_in sender_addr;
                    socklen_t sender_addr_len = sizeof(sender_addr);
                    struct NET_JOIN_PDU net_join = {0};

                    int rcv_data = recvfrom(node->sockfd_a, &net_join, sizeof(net_join), 0, (struct sockaddr *)&sender_addr, &sender_addr_len);
                    if (rcv_data == -1) {
                        perror("recvfrom failed");
                        continue;
                    }

                    //printf("Received NET_JOIN (Q6)\n");
                    //printf("Type (Q6): %d\n", net_join.type);
                    //printf("Source Address(Q6): %s\n", inet_ntoa((struct in_addr){.s_addr = net_join.src_address}));
                    //printf("Source Port(Q6): %d\n", net_join.src_port);
                    //printf("Max address(Q6): %s\n", inet_ntoa((struct in_addr){.s_addr = net_join.max_address}));
                    //printf("Max Port(Q6): %d\n", net_join.max_port);
                    //printf("we are abour to move to the q12 state from q6\n");
                    node->state_handler = state_handlers[7];
                    node->state_handler(node, &net_join);
                    return 0;

                }else if(poll_fd[i].fd == node->listener_socket){

                    printf("Received NET_JOIN_RESPONSE (Q6)\n");
                    struct sockaddr_in sender_addr;
                    socklen_t sender_addr_len = sizeof(sender_addr);

                    int accept_status = accept(node->listener_socket, (struct sockaddr*)&sender_addr, &sender_addr_len);
                    char buffer[1024];
                    int recv_status = recv(accept_status, buffer , sizeof(buffer), 0);
                    if (recv_status == -1){
                        perror("recv failed");
                        return 1;
                    }

                    //close(accept_status);
                    //close(node->listener_socket);            
                
                }
                else if(poll_fd[i].fd == node->sockfd_b){
                   // printf("we are about to recieve the NET_JOIN_RESPONSE from the state 5\n");
                   // we need to check the messahe disconnect from the predecessor.
                   

                }


                else if (poll_fd[i].fd == node->sockfd_d) {
                    printf("We are about to receive a message on sockfd_d\n");

                    // Buffer to store the raw data received
                    char buffer[1024] = {0};
                    memset(buffer, 0, sizeof(buffer));

                    // Receive the raw data
                    int recv_status = recv(node->sockfd_d, buffer, sizeof(buffer), 0);

                    if (recv_status == -1) {
                        perror("recv failed");
                        return 1;
                    }

                    // Validate that at least one byte (message type) has been received
                    if (recv_status < sizeof(uint8_t)) {
                        printf("Received incomplete message\n");
                        return 1;
                    }

                    // Extract the message type from the buffer
                    uint8_t message_type = *(uint8_t *)buffer;

                    if (message_type == NET_JOIN) {
                        // Validate that the full NET_JOIN_PDU was received
                        if (recv_status >= sizeof(struct NET_JOIN_PDU)) {
                            struct NET_JOIN_PDU *net_join = (struct NET_JOIN_PDU *)buffer;

                            printf("Received NET_JOIN message in STATE 6:\n");
                            printf("  Source Address: %s\n", inet_ntoa((struct in_addr){.s_addr = net_join->src_address}));
                            printf("  Source Port: %d\n", ntohs(net_join->src_port));
                            printf("  Max Address: %s\n", inet_ntoa((struct in_addr){.s_addr = net_join->max_address}));
                            printf("  Max Port: %d\n", ntohs(net_join->max_port));
                            printf("  Max Span: %u\n", ntohl(net_join->max_span));

                            // Transition to STATE_12 with the received NET_JOIN_PDU
                            node->state_handler = state_handlers[STATE_12];
                            node->state_handler(node, net_join);
                        } else {
                            printf("Incomplete NET_JOIN message received, likely due to padding or fragmentation.\n");
                        }
                    } else if (message_type == NET_CLOSE_CONNECTION) {
                        // Validate that the full NET_CLOSE_CONNECTION_PDU was received
                        if (recv_status >= sizeof(struct NET_CLOSE_CONNECTION_PDU)) {
                            struct NET_CLOSE_CONNECTION_PDU *net_close = (struct NET_CLOSE_CONNECTION_PDU *)buffer;

                            printf("Received NET_CLOSE_CONNECTION message in STATE 6\n");

                            // Transition to STATE_17 with the received NET_CLOSE_CONNECTION_PDU
                            node->state_handler = state_handlers[STATE_17];
                            node->state_handler(node, net_close);
                        } else {
                            printf("Incomplete NET_CLOSE_CONNECTION message received, likely due to padding or fragmentation.\n");
                        }
                    } else {
                        // Unknown message type
                        printf("Received an unknown message type: %d\n", message_type);
                    }
                }


                
                // TODO: I need to ensure that buffer that receives the message is correctly sized to receive the message
                // or probably the big-little endianess is causing the issue.
          
            }
        }
    }

    return 0;

}
