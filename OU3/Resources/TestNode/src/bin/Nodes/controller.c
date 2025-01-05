#include "controller.h"
#define UPD_BUFFER_SIZE 2048  
#define INITIAL_BYTE_READ_SIZE 1


// if the node is not connected after receiving sigint signal, we will close the connection.
// we may need to fix the close connection.

static void manage_pdu(Node* node, PDU* pdu);
ssize_t get_packet_size(uint8_t pdu_type, const char *buffer, size_t buffer_fill);
void deserialize_net_new_range(struct NET_NEW_RANGE_PDU* net_join , char* buffer);
void deserialize_net_leave(struct NET_LEAVING_PDU* net_leave, const char* buffer, size_t buffer_size);
static void process_queue(Node* node);
void deserialize_net_join(struct NET_JOIN_PDU* net_join, const char* buffer);



// based on the graph, I(gazi) think that state_6 kinda core since most states are being handled from here.
int q6_state(void* n, void* data) {
    Node* node = (Node*)n;
    printf("[q6 state]\n");

    static char udp_buffer[4096];
    static char successor_buffer[1024 * 1024];
    static char predecessor_buffer[1024 * 1024];
    static size_t udp_buffer_filled = 0;
    static size_t succ_buufer_filled = 0;
    static size_t pred_buffer_filled = 0;

    struct NET_ALIVE_PDU net_alive = {0};
    net_alive.type = NET_ALIVE;

    int timeout = 1;
    time_t last_time = time(NULL);
    struct pollfd poll_fd[4];
    poll_fd[0].fd = node->sockfd_a;
    poll_fd[0].events = POLLIN;
    int num_fds = 1;

    if (node->sockfd_b > 0 && node->sockfd_d < 0) {
        poll_fd[1].fd = node->sockfd_b;
        poll_fd[1].events = POLLIN;
        num_fds = 2;
    } else if (node->sockfd_d > 0 && node->sockfd_b < 0) {
        poll_fd[1].fd = node->sockfd_d;
        poll_fd[1].events = POLLIN;
        num_fds = 2;
    } else if (node->sockfd_b > 0 && node->sockfd_d > 0) {
        poll_fd[1].fd = node->sockfd_b;
        poll_fd[1].events = POLLIN;
        poll_fd[2].fd = node->sockfd_d;
        poll_fd[2].events = POLLIN;
        num_fds = 3;
    }

    while (1) {
        time_t current_time = time(NULL);

        if (current_time - last_time >= timeout) {
            printf("the number of entries in the hash table is %d\n", node->hash_table->length);
            int send_status = sendto(node->sockfd_a, &net_alive, sizeof(net_alive), 0, node->tracker_addr->ai_addr, node->tracker_addr->ai_addrlen);
            if (send_status == -1) {
                perror("send failure");
                return 1;
            }

            last_time = current_time;
            timeout = 3;
        }

        int poll_status = poll(poll_fd, num_fds, 1000);

        if (poll_status == -1 && errno != EINTR) {
            perror("poll failed");
            return 1;
        }

        if (should_close) {
            node->state_handler = state_handlers[STATE_10];
            node->state_handler(node, NULL);
            return 0;
        }

        for (int i = 0; i < num_fds; i++) {
            if (poll_fd[i].revents & POLLIN) {
                if (poll_fd[i].fd == node->sockfd_a) {
                    int bytes_recv = recvfrom(node->sockfd_a, udp_buffer, sizeof(udp_buffer), 0, NULL, NULL);
                    if (bytes_recv == -1) {
                        perror("recvfrom failure");
                        return 1;
                    }

                    udp_buffer_filled += bytes_recv;

                    struct PDU pdu;
                    pdu.type = udp_buffer[0];
                    pdu.data = udp_buffer;
                    memcpy(pdu.buffer, udp_buffer, bytes_recv);
                    manage_pdu(node, &pdu);
                } else if (poll_fd[i].fd == node->sockfd_b) {
                    bool time_to_close = false;

                    while (!time_to_close) {
                        ssize_t bytes_recv = recv(node->sockfd_b, successor_buffer + succ_buufer_filled, 1, MSG_DONTWAIT);

                        if (bytes_recv == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            } else {
                                time_to_close = true;
                            }
                        } else if (bytes_recv == 0) {
                            time_to_close = true;
                            close(node->sockfd_b);
                            node->sockfd_b = -1;
                            break;
                        } else {
                            succ_buufer_filled += bytes_recv;
                        }
                    }

                    size_t offset = 0;

                    while (succ_buufer_filled > 0) {
                        uint8_t pdu_type = successor_buffer[offset];
                        ssize_t pdu_size = get_packet_size(pdu_type, successor_buffer + offset, succ_buufer_filled);

                        if (pdu_size < 0) {
                            break;
                        }

                        if ((ssize_t)succ_buufer_filled < pdu_size) {
                            break;
                        }

                        PDU* pdu = (PDU*)malloc(sizeof(PDU));
                        if (!pdu) {
                            perror("malloc failed");
                            return 1;
                        }
                        pdu->size = pdu_size;
                        pdu->type = pdu_type;
                        pdu->data = successor_buffer + offset;
                        memcpy(pdu->buffer, successor_buffer + offset, pdu_size);
                        queue_enqueue(node->queue, pdu);
                        memset(successor_buffer + offset, 0, pdu_size);
                        offset += pdu_size;
                        succ_buufer_filled -= pdu_size;
                    }

                    process_queue(node);
                } else if (poll_fd[i].fd == node->sockfd_d) {
                    bool time_to_close = false;

                    while (!time_to_close) {
                        ssize_t bytes_recv = recv(node->sockfd_d, predecessor_buffer + pred_buffer_filled, 1, MSG_DONTWAIT);

                        if (bytes_recv == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            } else {
                                time_to_close = true;
                            }
                        } else if (bytes_recv == 0) {
                            time_to_close = true;
                            close(node->sockfd_d);
                            node->sockfd_d = -1;
                            break;
                        } else {
                            pred_buffer_filled += bytes_recv;
                        }
                    }

                    size_t offset = 0;
                    while (pred_buffer_filled > 0) {
                        uint8_t pdu_type = predecessor_buffer[offset];
                        ssize_t pdu_size = get_packet_size(pdu_type, predecessor_buffer + offset, pred_buffer_filled);

                        if (pdu_size < 0) {
                            break;
                        }

                        if ((ssize_t)pred_buffer_filled < pdu_size) {
                            break;
                        }

                        PDU* pdu = (PDU*)malloc(sizeof(PDU));
                        if (!pdu) {
                            perror("malloc failed");
                            return 1;
                        }
                        pdu->size = pdu_size;
                        pdu->type = pdu_type;
                        pdu->data = predecessor_buffer + offset;
                        memcpy(pdu->buffer, predecessor_buffer + offset, pdu_size);
                        queue_enqueue(node->queue, pdu);
                        memset(predecessor_buffer + offset, 0, pdu_size);
                        offset += pdu_size;
                        pred_buffer_filled -= pdu_size;
                    }

                    process_queue(node);
                }

                // Ensure that all allocated memory is freed
                while (!queue_is_empty(node->queue)) {
                    PDU* pdu = queue_dequeue(node->queue);
                    if (pdu) {
                        free(pdu);
                    }
                }
            }
        }
    }

    return 0;
}

void deserialize_net_join(struct NET_JOIN_PDU* net_join, const char* buffer){
    size_t offset = 0;

    net_join->type = (uint8_t) buffer[offset];
    offset += sizeof(uint8_t);

    memcpy(&net_join->src_address, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    memcpy(&net_join->src_port, buffer + offset, sizeof(uint16_t));
    // before we convert the port to the host order, we have to increment the offset.
    //printf("before ntohs: raw port = %u\n", (unsigned)net_join->src_port); 
    net_join->src_port = ntohs(net_join->src_port);
    //printf("after ntohs: port = %u\n", (unsigned)net_join->src_port);

    offset += sizeof(uint16_t);

    net_join->max_span = (uint8_t) buffer[offset];
    offset += sizeof(uint8_t);

    memcpy(&net_join->max_address, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    memcpy(&net_join->max_port, buffer + offset, sizeof(uint16_t));
    net_join->max_port = ntohs(net_join->max_port);
    offset += sizeof(uint16_t);
}

static void process_queue(Node* node) {
    while (!queue_is_empty(node->queue)) {
        PDU* pdu = queue_dequeue(node->queue);

        if (pdu) {
            //printf("Processing PDU at address %p\n", (void*)pdu);
            manage_pdu(node, pdu);
            //printf("Freeing PDU at address %p\n", (void*)pdu);
            free(pdu); // Free the PDU after processing
        }
    }
}



static void manage_pdu(Node* node, PDU* pdu){
    
    switch(pdu->type){
        case NET_LEAVING:
            struct NET_LEAVING_PDU net_leave = {0};
            deserialize_net_leave(&net_leave, pdu->buffer, pdu->size);
            //process_queue(node);
            //printf("Queue size before going to state 16: %d\n", node->queue->size);
            node->state_handler = state_handlers[STATE_16];
            node->state_handler(node, &net_leave);
            //printf("Queue size after going to state 16: %d\n", node->queue->size);
            break;
        case NET_NEW_RANGE:
            struct NET_NEW_RANGE_PDU net_new_range = {0};
            deserialize_net_new_range(&net_new_range, pdu->buffer);
            //process_queue(node);
            node->state_handler = state_handlers[STATE_15];
            node->state_handler(node, &net_new_range);
            break;
        case VAL_INSERT:
        case VAL_REMOVE:
        case VAL_LOOKUP:
            node->state_handler = state_handlers[STATE_9];
            node->state_handler(node, pdu);
            break;
        case NET_JOIN:
            // we will convert the fields to the host order.
            struct NET_JOIN_PDU net_join;
                
            //printf("NET_JOIN PDU raw buffer (size=%zd):\n", pdu->size);
            //for (size_t i = 0; i < sizeof(net_join); i++) {
              //  printf("%02x ", pdu->buffer[i]);
            //}
            //printf("\n");
            // now we will deserialize the net_join

            deserialize_net_join(&net_join, pdu->buffer);
            //printf("the port about to be send%d\n", net_join.src_port);

            node->state_handler = state_handlers[STATE_12];
            node->state_handler(node, &net_join);
            break;
        case NET_CLOSE_CONNECTION:
            node->state_handler = state_handlers[STATE_17];
            node->state_handler(node, pdu);
            break;
        default:
            printf("Unknown PDU type: %u\n", pdu->type);
            break;

    }

}

ssize_t get_packet_size(uint8_t pdu_type, const char *buffer, size_t buffer_fill){

    switch (pdu_type){

        case NET_ALIVE:
            // total byes of the NET_ALIVE_PDU
            return 1;

        case NET_GET_NODE:
            // total 1 bytes
            return 1;

        case NET_GET_NODE_RESPONSE:
            return 1 + 4 + 2;

        case NET_JOIN:
            return 1 + 4 + 2 + 1 + 4 + 2;

        case NET_JOIN_RESPONSE:
            return 1+ 4 + 2 + 1+ 1;

        case NET_CLOSE_CONNECTION:
            return 1;

        case NET_NEW_RANGE:
            return 1+ 1 + 1;

        case NET_NEW_RANGE_RESPONSE:
            return 1;

        case NET_LEAVING:
            return 1 + 4 + 2;

        case VAL_REMOVE:
            if(buffer_fill < 13){
                return -1;
            }
            return 1 + 12;

        case VAL_LOOKUP:
            if(buffer_fill < 19){
                return -1;
            }
            return 19;

        case VAL_LOOKUP_RESPONSE:{

            if (buffer_fill < 14){              
                // 1 (TYPE) + 12 (SSN) + 1 (NAME_LENGTH)
                return -1; // Insufficient data
            }

            uint8_t name_length = buffer[13];
            if (buffer_fill < 14 + name_length + 1){              
                // +1 for EMAIL_LENGTH
                return -1; // Insufficient data
            }

            uint8_t email_length = buffer[14 + name_length];
            return 1 + 12 + 1 + name_length + 1 + email_length;
        }

        case VAL_INSERT:{

            if (buffer_fill < 14){              
                // 1 (TYPE) + 12 (SSN) + 1 (NAME_LENGTH)
                return -1; // Insufficient data
            }

            uint8_t name_length = buffer[13];
            if (buffer_fill < 14 + name_length + 1){
                // +1 for EMAIL_LENGTH
                return -1; // Insufficient data
            }

            uint8_t email_length = buffer[14 + name_length];
            return 1 + 12 + 1 + name_length + 1 + email_length;

        }

        case STUN_LOOKUP:
            return sizeof(struct STUN_LOOKUP_PDU);

        case STUN_RESPONSE:
            return sizeof(struct STUN_RESPONSE_PDU);
        default:
            return -1;
    }
}


void deserialize_net_new_range(struct NET_NEW_RANGE_PDU* net_join , char* pdu){
    size_t offset = 0;
    net_join->type = pdu[offset++];
    net_join->range_start = pdu[offset++];
    net_join->range_end = pdu[offset++];
}


void deserialize_net_leave(struct NET_LEAVING_PDU* net_leave, const char* buffer, size_t buffer_size){

  
    size_t offset = 0;
    net_leave->type = buffer[offset++];
    memcpy(&net_leave->new_address, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    // we shall have the port number in the host order.
    memcpy(&net_leave->new_port, buffer + offset, sizeof(uint16_t));
    net_leave->new_port = ntohs(net_leave->new_port);
    offset += sizeof(uint16_t);

    return;
    

}
