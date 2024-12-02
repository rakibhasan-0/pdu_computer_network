#include "controller.h"

// based on the graph, I(gazi) think that state_6 kinda core since most states are being handled from here.
int q6_state(void* n, void* data){

    Node* node = (Node*)n;
    printf("q6 state\n");
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
            timeout = 6;
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

                if(poll_fd[i].fd == node->sockfd_a){
                  
                    struct sockaddr_in sender_addr;
                    socklen_t sender_addr_len = sizeof(sender_addr);
                    struct NET_JOIN_PDU net_join = {0};

                    int rcv_data = recvfrom(node->sockfd_a, &net_join, sizeof(net_join), 0, (struct sockaddr *)&sender_addr, &sender_addr_len);
                    if (rcv_data == -1) {
                        perror("recvfrom failed");
                        continue;
                    }

                    printf("Received NET_JOIN (Q6)\n");
                    printf("Type (Q6): %d\n", net_join.type);
                    printf("Source Address(Q6): %s\n", inet_ntoa((struct in_addr){.s_addr = net_join.src_address}));
                    printf("Source Port(Q6): %d\n", net_join.src_port);
                    printf("Max address(Q6): %s\n", inet_ntoa((struct in_addr){.s_addr = net_join.max_address}));
                    printf("Max Port(Q6): %d\n", net_join.max_port);
                    printf("we are abour to move to the q12 state from q6\n");
                    node->state_handler = state_handlers[7];
                    node->state_handler(node, &net_join);
                    return 0;

                }else if(poll_fd[i].fd == node->listener_socket){
                    struct sockaddr_in sender_addr;
                    socklen_t sender_addr_len = sizeof(sender_addr);

                    int accept_status = accept(node->listener_socket, (struct sockaddr*)&sender_addr, &sender_addr_len);
                    char buffer[1024];
                    int recv_status = recv(accept_status, buffer , sizeof(buffer), 0);
                    if (recv_status == -1){
                        perror("recv failed");
                        return 1;
                    }

                    if(buffer[0] == GET_SUCCESSOR){
                        printf("Received GET_SUCCESSOR request from predecessor.\n");
                        struct GET_SUCCESSOR_RESPONSE_PDU get_successor_response = {0};
                        get_successor_response.type = GET_SUCCESSOR_RESPONSE;
                        get_successor_response.successor_address = node->successor_ip_address.s_addr;
                        get_successor_response.successor_port = htons(node->successor_port);

                        int send_status = send(accept_status, &get_successor_response, sizeof(get_successor_response), 0);
                        if (send_status == -1) {
                            perror("send to predecessor failed");
                            return 1;
                        }

                        printf("Sent GET_SUCCESSOR_RESPONSE to predecessor.\n");
                    }
                    //close(accept_status);
                    //close(node->listener_socket);
            
                
                }
                else if(poll_fd[i].fd == node->sockfd_b){
                    printf("we are about to recieve the NET_JOIN_RESPONSE from the state 5\n");
                }

                else if(poll_fd[i].fd == node->sockfd_d){
                   printf("we are about to recieve the GET_SUCCESSOR_RESPONSE from the state 8\n");
                }
                    
            }
        }
    }

    return 0;

}
