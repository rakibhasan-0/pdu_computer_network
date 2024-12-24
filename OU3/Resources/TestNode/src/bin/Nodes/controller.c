#include "controller.h"
#define UPD_BUFFER_SIZE 2048  
#define INITIAL_BYTE_READ_SIZE 1


// if the node is not connected after receiving sigint signal, we will close the connection.
// we may need to fix the close connection.

static void manage_pdu(Node* node, PDU* pdu);
ssize_t get_packet_size(uint8_t pdu_type, const char *buffer, size_t buffer_fill);
void deserialize_net_new_range(struct NET_NEW_RANGE_PDU* net_join , char* buffer);
void deserialize_net_leave(struct NET_LEAVING_PDU* net_leave, const char* buffer, size_t buffer_size);

void print_buffer_hex(const char* buffer, size_t buffer_size){
    for(size_t i = 0; i < buffer_size; i++){
        printf("%02x ", (unsigned char)buffer[i]);
    }
    printf("\n");
}




// based on the graph, I(gazi) think that state_6 kinda core since most states are being handled from here.
int q6_state(void* n, void* data){

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
   
    node->is_alive = true;
    queue_t *q = queue_create(10);
    int total_received_messages = 0;
    
    // time interval for the alive message
    int timeout = 1;
    time_t last_time = time(NULL);
    struct pollfd poll_fd[4];
    poll_fd[0].fd = node->sockfd_a;
    poll_fd[0].events = POLLIN;
    poll_fd[1].fd = node->sockfd_b;
    poll_fd[1].events = POLLIN;
    poll_fd[2].fd = node->sockfd_d;
    poll_fd[2].events = POLLIN;
 

    while(1){
        time_t current_time = time(NULL);

        if (current_time - last_time >= timeout){
            printf("the number of entries in the hash table is %d\n", node->hash_table->length);
            int send_status = sendto(node->sockfd_a, &net_alive, sizeof(net_alive), 0,
                                     node->tracker_addr->ai_addr, node->tracker_addr->ai_addrlen);
            if (send_status == -1){
                perror("send failure");
                return 1;
            }


            last_time = current_time;
            timeout = 3;
        }

       // printf("the node have %d entries", ht_get_entry_count(node->hash_table));
        int poll_status = poll(poll_fd, 3, 4000);

        if (poll_status == -1 && errno != EINTR) {
            perror("poll failed");
            return 1;
        }

        if (should_close) {
            node->state_handler = state_handlers[STATE_10];
            node->state_handler(node, NULL);
            return 0;
        }


        for(int i = 0; i < 3; i++){
            
            if(poll_fd[i].revents == POLLIN){
                
                // udp socket
                if(poll_fd[i].fd == node->sockfd_a){

                    int bytes_recv = recvfrom(node->sockfd_a, udp_buffer, sizeof(udp_buffer), 0, NULL, NULL); 
                    if(bytes_recv == -1){
                        perror("recvfrom failure");
                        return 1;
                    }

                    udp_buffer_filled += bytes_recv;

                    struct PDU pdu;
                    pdu.type = udp_buffer[0];
                    pdu.data = udp_buffer;
                    //printf("PDU type: %u\n", pdu.type);
                    memcpy(pdu.buffer, udp_buffer, bytes_recv);
                    manage_pdu(node, &pdu);

                    if(!queue_is_empty(node->queue_for_values)){
                        node->state_handler = state_handlers[STATE_9];
                        node->state_handler(node, NULL);
                    }

                    printf("UDP buffer filled: %zu\n", udp_buffer_filled);

                }

                // TCP socket for the successor 
                else if(poll_fd[i].fd == node->sockfd_b){
                   
                    bool close = false;

                    // we are reading all of the bytes.
                    while(!close){
                        
                        ssize_t bytes_recv = recv(node->sockfd_b, successor_buffer + succ_buufer_filled, 1,
                                                    MSG_DONTWAIT);

                        printf("bytes received successor: %zd\n", bytes_recv);

                        if(bytes_recv == -1){
                            if(errno == EAGAIN || errno == EWOULDBLOCK){
                                break;
                            }
                            else{
                                close = true;
                            }
                        }
                        else if(bytes_recv == 0){
                            close = true;            
                        }else{
                            succ_buufer_filled += bytes_recv;
                        }

                    }

                    size_t offset = 0;

                    while (succ_buufer_filled > 0){

                        uint8_t pdu_type = successor_buffer[offset];
                        ssize_t pdu_size = get_packet_size(pdu_type, successor_buffer + offset, succ_buufer_filled);

                        if(pdu_size == 0){
                            break;
                        }

                        if((ssize_t)succ_buufer_filled < pdu_size){
                            break;
                        }

                        PDU* pdu = malloc(sizeof(PDU));
                        //pdu->buffer = malloc(pdu_size * sizeof(char));
                        printf("PDU type: %u, size: %zd\n", pdu_type, pdu_size);
                        pdu->size = pdu_size;
                        pdu->type = pdu_type;
                        pdu->data = successor_buffer + offset;
                        memcpy(pdu->buffer, successor_buffer + offset, pdu_size);
                        queue_enqueue(q, pdu);
                        memset(successor_buffer+offset, 0, pdu_size);
                        offset += pdu_size;
                        succ_buufer_filled -= pdu_size;

                    }

                    while(!queue_is_empty(q)){
                        PDU* pdu = queue_dequeue(q);
                        manage_pdu(node, pdu);
                        free(pdu);
                    }

                }


        
                // predecessor socket
                else if (poll_fd[i].fd == node->sockfd_d) {
                    bool close = false;

                        // 1) Read as many bytes as are available right now (non-blocking).
                        while (!close) {
                            ssize_t bytes_recv = recv(node->sockfd_d,
                                                    predecessor_buffer + pred_buffer_filled,
                                                    1,
                                                    MSG_DONTWAIT);

                            printf("bytes received predecessor: %zd\n", bytes_recv);

                            if (bytes_recv == -1) {
                                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                    // No more data at this moment, break from read loop
                                    break;
                                } else {
                                    // Some other error happened -> we should close the connection
                                    perror("recv error on sockfd_d");
                                    close = true;
                                }
                            } else if (bytes_recv == 0) {
                                // Peer closed connection
                                close = true;
                            } else {
                                // We read some data
                                pred_buffer_filled += bytes_recv;
                            }
                        }

                        close = false;

                        // 2) Now parse whatever is in `predecessor_buffer`.
                        //    We can extract mu

                        printf("filled buffer size: %zu\n", pred_buffer_filled);

                        size_t offset = 0;

                        while (pred_buffer_filled > 0) {
                            uint8_t pdu_type = predecessor_buffer[offset];
                            ssize_t pdu_size = get_packet_size(pdu_type, predecessor_buffer + offset, pred_buffer_filled);

                            if (pdu_size == 0) {
                                break;
                            }

                            if ((ssize_t)pred_buffer_filled < pdu_size) {
                                break;
                            }

                            PDU* pdu = malloc(sizeof(PDU));
                            pdu->size = pdu_size;
                            pdu->type = pdu_type;
                            pdu->data = predecessor_buffer + offset;
                            memcpy(pdu->buffer, predecessor_buffer + offset, pdu_size);
                            queue_enqueue(q, pdu);
                            memset(predecessor_buffer + offset, 0, pdu_size);
                            offset += pdu_size;
                            pred_buffer_filled -= pdu_size;
                        }

                        printf("Queue size: %d\n", q->size);

                        while (!queue_is_empty(q)) {
                            PDU* p = queue_dequeue(q);
                            manage_pdu(node, p);
                        }

                    



                    
                }
       
            }
        }

    }

    return 0;

}




static void manage_pdu(Node* node, PDU* pdu){
    
    switch(pdu->type){
        case NET_LEAVING:
            struct NET_LEAVING_PDU net_leave = {0};
            deserialize_net_leave(&net_leave, pdu->buffer, pdu->size);
            node->state_handler = state_handlers[STATE_16];
            node->state_handler(node, &net_leave);
            break;
        case NET_NEW_RANGE:
            struct NET_NEW_RANGE_PDU net_new_range = {0};
            deserialize_net_new_range(&net_new_range, pdu->buffer);
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
            struct NET_JOIN_PDU* net_join = (struct NET_JOIN_PDU*)pdu->buffer;
            net_join->src_address = net_join->src_address;
            net_join->src_port = ntohs(net_join->src_port);
            net_join->max_address = net_join->max_address;
            net_join->max_port = ntohs(net_join->max_port);
            net_join->max_span = net_join->max_span;

            node->state_handler = state_handlers[STATE_12];
            node->state_handler(node, net_join);
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
        return sizeof(struct NET_ALIVE_PDU);

    case NET_GET_NODE:
        return sizeof(struct NET_GET_NODE_PDU);

    case NET_GET_NODE_RESPONSE:
        return sizeof(struct NET_GET_NODE_RESPONSE_PDU);

    case NET_JOIN:
        return sizeof(struct NET_JOIN_PDU);

    case NET_JOIN_RESPONSE:
        return sizeof(struct NET_JOIN_RESPONSE_PDU);

    case NET_CLOSE_CONNECTION:
        return sizeof(struct NET_CLOSE_CONNECTION_PDU);

    case NET_NEW_RANGE:
        return sizeof(struct NET_NEW_RANGE_PDU);

    case NET_NEW_RANGE_RESPONSE:
        return sizeof(struct NET_NEW_RANGE_RESPONSE_PDU);

    case NET_LEAVING:
        return sizeof(struct NET_LEAVING_PDU);

    case VAL_REMOVE:
        return sizeof(struct VAL_REMOVE_PDU);

    case VAL_LOOKUP:
        return sizeof(struct VAL_LOOKUP_PDU);

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

    if(buffer_size < sizeof(struct NET_LEAVING_PDU)){
        return;
    }

    print_buffer_hex(buffer, buffer_size);
    size_t offset = 0;

    net_leave->type = buffer[offset];
    offset += 4;

    memcpy(&net_leave->new_address, buffer + offset, sizeof(uint32_t));

    offset += 4;
    memcpy(&net_leave->new_port, buffer + offset, sizeof(uint16_t));

    printf("NET_LEAVING_PDU: type=%u, new_address=%u, new_port=%u\n", net_leave->type, net_leave->new_address, ntohs(net_leave->new_port));


}
