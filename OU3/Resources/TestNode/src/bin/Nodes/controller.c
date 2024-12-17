#include "controller.h"


// if the node is not connected after receiving sigint signal, we will close the connection.






// based on the graph, I(gazi) think that state_6 kinda core since most states are being handled from here.
int q6_state(void* n, void* data){

    Node* node = (Node*)n;
    printf("[q6 state]\n");

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

       // printf("the node have %d entries", ht_get_entry_count(node->hash_table));
        int poll_status = poll(poll_fd, 4, 100);

        if (poll_status == -1 && errno != EINTR) {
            perror("poll failed");
            return 1;
        }

        if (should_close) {
            node->state_handler = state_handlers[STATE_10];
            node->state_handler(node, NULL);
            return 0;
        }


        for(int i = 0; i < 4; i++){

            if(poll_fd[i].revents == POLLIN){
                // clearning the revents
                poll_fd[i].revents = 0;

               //printf("Received something\n");

                if(poll_fd[i].fd == node->sockfd_a){
				    char buffer[1024];
					struct sockaddr_in sender_addr;
					socklen_t sender_addr_len = sizeof(sender_addr);
					int recv_status = recvfrom(node->sockfd_a, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
					if (recv_status == -1) {
						perror("recvfrom failed");
						return 1;
					}

					uint8_t pdu_type = buffer[0]; // the first byte indicates the PDU type
                    printf("the pdu type is: %d\n", pdu_type);
                    printf("the size of the buffer is: %d\n", sizeof(buffer));
                    printf("the size of the net_join is: %d\n", sizeof(struct NET_JOIN_PDU));
					/*switch (pdu_type) {
						case VAL_INSERT:
						case VAL_REMOVE:
						case VAL_LOOKUP:
							node->state_handler = state_handlers[STATE_9];
							node->state_handler(node, buffer);
							break;
						default:
							printf("Unknown PDU type: %d\n", pdu_type);
							break;	
					}*/

                    // just wondering, we will check the pdu type send it to the state 9?
                    if(pdu_type == VAL_INSERT || pdu_type == VAL_REMOVE || pdu_type == VAL_LOOKUP){
                        node->state_handler = state_handlers[STATE_9];
                        node->state_handler(node, buffer);
                    }

                    if(pdu_type == NET_JOIN){
                        
                        struct NET_JOIN_PDU net_join = (*(struct NET_JOIN_PDU*)buffer);

                        // convert the fields to the host order.
                        printf("Received NET_JOIN (Q6) via UDP\n");
                        net_join.src_address = net_join.src_address;
                        net_join.src_port = ntohs(net_join.src_port);
                        net_join.max_address = net_join.max_address;
                        net_join.max_port = ntohs(net_join.max_port);
                        net_join.max_span = net_join.max_span;

                        //printf("Received NET_JOIN (Q6)\n");
                        //printf("Type (Q6): %d\n", net_join.type);
                        printf("Source Address(Q6): %s\n", inet_ntoa((struct in_addr){.s_addr = net_join.src_address}));
                        printf("Source Port(Q6): %d\n", net_join.src_port);
                        //printf("Max address(Q6): %s\n", inet_ntoa((struct in_addr){.s_addr = net_join.max_address}));
                        //printf("Max Port(Q6): %d\n", net_join.max_port);
                        //printf("we are abour to move to the q12 state from q6\n");
                        node->state_handler = state_handlers[STATE_12];
                        node->state_handler(node, &net_join);

                    }


                }else if(poll_fd[i].fd == node->listener_socket){

                    printf("Received NET_JOIN_RESPONSE (Q6) through the listener socket\n");
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
                    // it is empty so far, but I guess we may need to handle NET_NEW_RANGE message here?
                    // we shall catch the NET_CLOSE_CONNECTION message here, since we send the net_leave message from the node's successor.
                    char buffer[1024];
                    memset(buffer, 0, sizeof(buffer));
                    int recv_status = recv(node->sockfd_b, buffer, sizeof(buffer), 0);
                    if (recv_status == -1) {
                        perror("recv failed");
                        return 1;
                    }

                    uint8_t message_type = *(uint8_t *)buffer;
                    if(message_type == NET_LEAVING){
                        node->state_handler = state_handlers[STATE_16];
                        node->state_handler(node, buffer);
                    }

                    if(message_type == NET_NEW_RANGE){
                        printf("Received NET_NEW_RANGE message in STATE 6\n");
                        printf("we are sending data to the STATE 15\n");
                        // we are moving to the state 15.
                        node->state_handler = state_handlers[STATE_15];
                        node->state_handler(node, buffer);
                    }

                    if(message_type == VAL_INSERT || message_type == VAL_REMOVE || message_type == VAL_LOOKUP){
                        node->state_handler = state_handlers[STATE_9];
                        node->state_handler(node, buffer);
                    }

                }


                else if (poll_fd[i].fd == node->sockfd_d) {

                    // Buffer to store the raw data received
                    char buffer[1024] = {0};
                    memset(buffer, 0, sizeof(buffer));

                    // Receive the raw data
                    int recv_status = recv(node->sockfd_d, buffer, sizeof(buffer), 0);

                    if (recv_status == -1) {
                        perror("recv failed");
                        return 1;
                    }


                    // Extract the message type from the buffer
                    uint8_t message_type = *(uint8_t *)buffer;

                    if (message_type == NET_JOIN) {
                        // Validate that the full NET_JOIN_PDU was received
                        if (recv_status >= sizeof(struct NET_JOIN_PDU)) {
                            struct NET_JOIN_PDU *net_join = (struct NET_JOIN_PDU *)buffer;
                            // Transition to STATE_12 with the received NET_JOIN_PDU

                            // we have received the NET_JOIN message from the predecessor.
                            printf("Received NET_JOIN message from the predecessor\n");
                            // we will convert the fields to the host order.
                            net_join->src_address = net_join->src_address;
                            net_join->src_port = ntohs(net_join->src_port);
                            net_join->max_address = net_join->max_address;
                            net_join->max_port = ntohs(net_join->max_port);
                            net_join->max_span = net_join->max_span;

                            node->state_handler = state_handlers[STATE_12];
                            node->state_handler(node, net_join);
                        }
                    } else if (message_type == NET_CLOSE_CONNECTION) {
                        // Validate that the full NET_CLOSE_CONNECTION_PDU was received
                        if (recv_status >= sizeof(struct NET_CLOSE_CONNECTION_PDU)) {
                            struct NET_CLOSE_CONNECTION_PDU *net_close = (struct NET_CLOSE_CONNECTION_PDU *)buffer;

                            printf("Received NET_CLOSE_CONNECTION message in STATE 6\n");

                            // Transition to STATE_17 with the received NET_CLOSE_CONNECTION_PDU
                            node->state_handler = state_handlers[STATE_17];
                            node->state_handler(node, net_close);
                        }
                    }

                    else if(message_type == NET_NEW_RANGE){
                        // we will update the new hash range.
                        printf("Received NET_NEW_RANGE message in STATE 6\n");
                        printf("we are sending data to the STATE 15\n");
                        // we are moving to the state 15.
                        node->state_handler = state_handlers[STATE_15];
                        node->state_handler(node, buffer);
                    }

                    else if(message_type == VAL_INSERT || message_type == VAL_REMOVE || message_type == VAL_LOOKUP){
                        node->state_handler = state_handlers[STATE_9];
                        node->state_handler(node, buffer);
                    }
                }
                
                // TODO: I need to ensure that buffer that receives the message is correctly sized to receive the message
                // or probably the big-little endianess is causing the issue.
          
            }
        }

    }

    return 0;

}
