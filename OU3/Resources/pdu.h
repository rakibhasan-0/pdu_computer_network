#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define SSN_LENGTH 12

#define NET_ALIVE 0
#define NET_GET_NODE 1
#define NET_GET_NODE_RESPONSE 2
#define NET_JOIN 3
#define NET_JOIN_RESPONSE 4
#define NET_CLOSE_CONNECTION 5

#define NET_NEW_RANGE 6
#define NET_LEAVING 7
#define NET_NEW_RANGE_RESPONSE 8

#define VAL_INSERT 100
#define VAL_REMOVE 101
#define VAL_LOOKUP 102
#define VAL_LOOKUP_RESPONSE 103

#define STUN_LOOKUP 200
#define STUN_RESPONSE 201


#ifndef PDU_DEF
#define PDU_DEF


#pragma pack(push, 1)
/**
* @brief NET_ALIVE_PDU (UDP)
*
* Sent from an alive node to the tracker to mark that it is alive and accepting UDP traffic on the specified PORT.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 0     |
*/
struct NET_ALIVE_PDU {
    uint8_t type;
};

/**
* @brief NET_GET_NODE_PDU (UDP)
*
* Sent from a prospect node to the tracker to get the first node of the network.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 1     |
*/
struct NET_GET_NODE_PDU {
    uint8_t type;
};

/**
* @brief NET_GET_NODE_RESPONSE_PDU (UDP)
*
* Sent from tracker to a prospect node as response to NET_GET_NODE_PDU.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 2     |
* | ADDRESS     | 4           |       |
* | PORT        | 2           |       |
*/
struct NET_GET_NODE_RESPONSE_PDU {
    uint8_t type;
    uint32_t address;
    uint16_t port;
};

/**
* @brief NET_JOIN_PDU (UDP/TCP)
*
* Sent from a node to join the network.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 3     |
* | SRC_ADDRESS | 4           |       |
* | SRC_PORT    | 2           |       |
* | MAX_SPAN    | 1           |       |
* | MAX_ADDRESS | 4           |       |
* | MAX_PORT    | 2           |       |
*/
struct NET_JOIN_PDU {
    uint8_t type;
    uint32_t src_address;
    uint16_t src_port;
    uint8_t max_span;
    uint32_t max_address;
    uint16_t max_port;
};

/**
* @brief NET_JOIN_RESPONSE_PDU (TCP)
*
* Sent from the final node of an insertion search to the prospect node.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 4     |
* | NEXT_ADDRESS| 4           |       |
* | NEXT_PORT   | 2           |       |
* | RANGE_START | 1           |       |
* | RANGE_END   | 1           |       |
*/
struct NET_JOIN_RESPONSE_PDU {
    uint8_t type;
    uint32_t next_address;
    uint16_t next_port;
    uint8_t range_start;
    uint8_t range_end;
};

/**
* @brief NET_CLOSE_CONNECTION_PDU (TCP)
*
* Signals that the connection should be dropped.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 5     |
*/
struct NET_CLOSE_CONNECTION_PDU {
    uint8_t type;
};

/**
* @brief NET_NEW_RANGE_PDU (TCP)
*
* Sent to update the hash range of a node.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 6     |
* | RANGE_START | 1           |       |
* | RANGE_END   | 1           |       |
*/
struct NET_NEW_RANGE_PDU {
    uint8_t type;
    uint8_t range_start;
    uint8_t range_end;
};

/**
* @brief NET_NEW_RANGE_RESPONSE_PDU (TCP)
*
* Sent as a response to NET_NEW_RANGE_PDU.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 8     |
*/
struct NET_NEW_RANGE_RESPONSE_PDU {
    uint8_t type;
};

/**
* @brief NET_LEAVING_PDU (TCP)
*
* Sent from a leaving node to its predecessor or successor.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 7     |
* | NEW_ADDRESS | 4           |       |
* | NEW_PORT    | 2           |       |
*/
struct NET_LEAVING_PDU {
    uint8_t type;
    uint32_t new_address; 
    uint16_t new_port;
};

/**
* @brief VAL_INSERT_PDU (TCP/UDP)
*
* Contains an entry that is to be inserted into the network.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 100   |
* | SSN         | 12          |       |
* | NAME_LENGTH | 1           |       |
* | NAME        | NAME_LENGTH |       |
* | EMAIL_LENGTH| 1           |       |
* | EMAIL       | EMAIL_LENGTH|       |
*/
struct VAL_INSERT_PDU {
    uint8_t type;
    uint8_t ssn[SSN_LENGTH];
    uint8_t name_length;
    uint8_t* name;
    uint8_t email_length;
    uint8_t* email;
};

/**
* @brief VAL_REMOVE_PDU (TCP/UDP)
*
* Contains an entry that is to be removed from the network.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 101   |
* | SSN         | 12          |       |
*/
struct VAL_REMOVE_PDU {
    uint8_t type;
    uint8_t ssn[SSN_LENGTH];
};

/**
* @brief VAL_LOOKUP_PDU (TCP/UDP)
*
* Contains a request for an entry.
* 
* | Field          | Bytes | Value |
* | -------------- | ----- | ----- |
* | TYPE           | 1     | 102   |
* | SSN            | 12    |       |
* | SENDER_ADDRESS | 4     |       |
* | SENDER_PORT    | 2     |       |
*/
struct VAL_LOOKUP_PDU {
    uint8_t type;
    uint8_t ssn[SSN_LENGTH];
    uint32_t sender_address;
    uint16_t sender_port;
};

/**
* @brief VAL_LOOKUP_RESPONSE_PDU (UDP)
*
* Sent as a response if a node can successfully match a VAL_LOOKUP_PDU.
* 
* | Field         | Bytes         | Value |
* | ------------- | ------------- | ----- |
* | TYPE          | 1             | 103   |
* | SSN           | 12            |       |
* | NAME_LENGTH   | 1             |       |
* | NAME          | NAME_LENGTH   |       |
* | EMAIL_LENGTH  | 1             |       |
* | EMAIL         | EMAIL_LENGTH  |       |
*/
struct VAL_LOOKUP_RESPONSE_PDU {
    uint8_t type;
    uint8_t ssn[SSN_LENGTH];
    uint8_t name_length;
    uint8_t* name;
    uint8_t email_length;
    uint8_t* email;
};

/**
* @brief STUN_LOOKUP_PDU (UDP)
*
* Can be sent to the tracker to discover one's public IP.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 200   |
*/
struct STUN_LOOKUP_PDU {
    uint8_t type;
};

/**
* @brief STUN_RESPONSE_PDU (UDP)
*
* Sent as a response to a STUN_LOOKUP_PDU.
* 
* | Field       | Bytes       | Value |
* |-------------|-------------|-------|
* | TYPE        | 1           | 201   |
* | ADDRESS     | 4           |       |
*/
struct STUN_RESPONSE_PDU {
    uint8_t type;
    uint32_t address;
};

#pragma pack(pop)
#endif // PDU_DEF
