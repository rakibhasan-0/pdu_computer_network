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
            printf("The number of entries in the hash table is %d\n", get_num_entries(node->hash_table));
            int send_status = sendto(node->sockfd_a, &net_alive, sizeof(net_alive), 0,
                                     node->tracker_addr->ai_addr, node->tracker_addr->ai_addrlen);
            last_time = current_time;
            timeout = 10;
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

                if(poll_fd[i].fd == node->sockfd_a){

                    while(true){
                        
                        char buffer[1024];
                        memset(buffer, 0, sizeof(buffer));
				        struct sockaddr_in sender_addr;
				        socklen_t sender_addr_len = sizeof(sender_addr);
                        // we will continue to receive the data until there is no data to receive.
                        int recv_status = recvfrom(node->sockfd_a, buffer, sizeof(buffer),0, (struct sockaddr*)&sender_addr,
                                                &sender_addr_len);
                
                        if (recv_status == -1) {
                            if(errno == EAGAIN || errno == EWOULDBLOCK){
                                break;
                            }
                        }
                        
                        if(recv_status == 0){
                            break;
                        }

                        uint8_t pdu_type = buffer[0]; 
                        // just wondering, we will check the pdu type send it to the state 9?
                        if(pdu_type == VAL_INSERT || pdu_type == VAL_REMOVE || pdu_type == VAL_LOOKUP){
                            node->state_handler = state_handlers[STATE_9];
                            node->state_handler(node, buffer);
                            break;
                        }

                        else if(pdu_type == NET_JOIN){     
                            printf("received NET_JOIN\n");                   
                            struct NET_JOIN_PDU net_join = (*(struct NET_JOIN_PDU*)buffer);
                            net_join.src_address = net_join.src_address;
                            net_join.src_port = ntohs(net_join.src_port);
                            net_join.max_address = net_join.max_address;
                            net_join.max_port = ntohs(net_join.max_port);
                            net_join.max_span = net_join.max_span;
                            node->state_handler = state_handlers[STATE_12];
                            node->state_handler(node, &net_join);
                            break;
                        }
                    }

                }else if(poll_fd[i].fd == node->listener_socket){
                    printf("hey, I am TCP listening socket\n");
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

                    while(1){

                        char buffer[1024];
                        memset(buffer, 0, sizeof(buffer));

                        int recv_status = recv(node->sockfd_b, buffer, sizeof(buffer),0);
                        if (recv_status == -1) {    
                            if(errno == EAGAIN || errno == EWOULDBLOCK){
                                break;
                            }
                        }

                        if(recv_status == 0){
                            break;
                        }

                        uint8_t pdu_type = buffer[0];

                        if(pdu_type == NET_LEAVING){
                            node->state_handler = state_handlers[STATE_16];
                            node->state_handler(node, buffer);
                            break;
                        }

                        if(pdu_type == NET_NEW_RANGE){
                            node->state_handler = state_handlers[STATE_15];
                            node->state_handler(node, buffer);
                            break;
                        }

                        if(pdu_type == VAL_INSERT || pdu_type == VAL_REMOVE || pdu_type == VAL_LOOKUP){
                            node->state_handler = state_handlers[STATE_9];
                            node->state_handler(node, buffer);
                            break;
                        }

                    }
                    
                }


                else if (poll_fd[i].fd == node->sockfd_d) {

                    // Buffer to store the raw data received
                    while(1){

                        char buffer[1024] = {0};
                        memset(buffer, 0, sizeof(buffer));

                        // Receive the raw data
                        int recv_status = recv(node->sockfd_d, buffer, sizeof(buffer), 0);

                        if(recv_status == -1){
                            if(errno == EAGAIN || errno == EWOULDBLOCK){
                                break;
                            }
                        }

                        if (recv_status == 0) {
                            break;
                        }

                        // Extract the message type from the buffer
                        uint8_t pdu_type = buffer[0];

                        if (pdu_type == NET_JOIN) {
                            // Validate that the full NET_JOIN_PDU was received
                            if (recv_status >= sizeof(struct NET_JOIN_PDU)) {
                                struct NET_JOIN_PDU *net_join = (struct NET_JOIN_PDU *)buffer;
                                // we will convert the fields to the host order.
                                net_join->src_address = net_join->src_address;
                                net_join->src_port = ntohs(net_join->src_port);
                                net_join->max_address = net_join->max_address;
                                net_join->max_port = ntohs(net_join->max_port);
                                net_join->max_span = net_join->max_span;

                                node->state_handler = state_handlers[STATE_12];
                                node->state_handler(node, net_join);
                                break;
                            }
                        } else if (pdu_type == NET_CLOSE_CONNECTION) {
                            if (recv_status >= sizeof(struct NET_CLOSE_CONNECTION_PDU)) {
                                struct NET_CLOSE_CONNECTION_PDU *net_close = (struct NET_CLOSE_CONNECTION_PDU *)buffer;
                                node->state_handler = state_handlers[STATE_17];
                                node->state_handler(node, net_close);
                                break;
                            }
                        }

                        else if(pdu_type == NET_NEW_RANGE){
                            node->state_handler = state_handlers[STATE_15];
                            node->state_handler(node, buffer);
                            break;
                        }

                        else if(pdu_type == VAL_INSERT || pdu_type == VAL_REMOVE || pdu_type == VAL_LOOKUP){
                            node->state_handler = state_handlers[STATE_9];
                            node->state_handler(node, buffer);
                            break;
                        }

                    }
                }        
            }
        }

    }

    return 0;

}
