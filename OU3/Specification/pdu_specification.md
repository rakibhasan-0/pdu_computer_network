# PDU Definitions

## NETWORK PDU ID 

| PDU                           | PROTOCOL  | ID/type | 
| ----------------------------- | --------- | --      | 
| NET\_ALIVE\_PDU               | UDP       | 0       |
| NET\_GET\_NODE\_PDU           | UDP       | 1       |
| NET\_GET\_NODE\_RESPONSE\_PDU | UDP       | 2       |
| NET\_JOIN\_PDU                | UDP / TCP | 3       |
| NET\_JOIN\_RESPONSE\_PDU      | TCP       | 4       |
| NET\_CLOSE\_CONNECTION\_PDU   | TCP       | 5       |
| NET\_NEW\_RANGE\_PDU          | TCP       | 6       |
| NET\_LEAVING\_PDU             | TCP       | 7       |
| NET\_NEW\_RANGE\_RESPONSE\_PDU| TCP       | 8       |


## VAL PDU ID
| PDU                           | PROTOCOL  | ID/type  | 
| ----------------------------- | --------- | --       | 
| VAL\_INSERT\_PDU              | UDP / TCP | 100      |
| VAL\_REMOVE\_PDU              | UDP / TCP | 101      |
| VAL\_LOOKUP\_PDU              | UDP / TCP | 102      |
| VAL\_LOOKUP\_RESPONSE\_PDU    | UDP       | 103      |


## STUN PDU ID
| PDU                           | PROTOCOL  | ID/type  | 
| ----------------------------- | --------- | --       | 
| STUN\_LOOKUP\_PDU:            | UDP       | 200      |
| STUN\_RESPONSE\_PDU           | UDP       | 201      |


## NET\_ALIVE\_PDU: (UDP)
Sent from an alive node to the tracker to mark that it is alive and accepting
UDP traffic on the specified PORT. The PORT is sent in network byte order.

| Field | Bytes |Value|
| ----- | ----- |-----|
| TYPE  | 1     |0    |


## NET\_GET\_NODE\_PDU: (UDP)
Sent from a prospect node to the tracker to get the first node of the network.

| Field | Bytes |Value|
| ----- | ----- |-----|
| TYPE  | 1     |1    |


## NET\_GET\_NODE\_RESPONSE\_PDU: (UDP)
Sent from tracker to a prospect node as response to NET\_GET\_NODE\_PDU.
The entire ADDRESS field and PORT are 0 if no node exists. The PORT is sent in network byte
order.

| Field   | Bytes |Value|
| -----   | ----- |-----|
| TYPE    | 1     |2    | 
| ADDRESS | 4     |     |
| PORT    | 2     |     |


## NET\_JOIN\_PDU: (UDP)/(TCP)
Sent from a prospect node to a joined node over UDP to start the joining
process. The SRC\_ADDRESS and SRC\_PORT are set to the prospects
address and accept port. 

The PDU MAX\_\* fields are updated by the receiver and forwarded over TCP by
the receiver until the node matching the max ip and port receives the PDU.
This triggers a NET\_JOIN\_RESPONSE\_PDU to the SRC\_\* by connecting to the prospect
as its succesor, this is explained in more detail in the state descriptions.
The \*\_PORT fields are sent in network byte order.

| Field       | Bytes |Value|
| ----------- | ----- |-----|
| TYPE        | 1     |3    |
| SRC\_ADDRESS| 4     |     |
| SRC\_PORT   | 2     |     |
| MAX\_SPAN   | 1     |     |
| MAX\_ADDRESS| 4     |     |
| MAX\_PORT   | 2     |     |


## NET\_JOIN\_RESPONSE\_PDU: (TCP)
Sent from the final node of an insertion search to the prospect node. The
NEXT\_\* is initialized to the senders next node. The range is initialized to
the hash-range the prospect should keep track of.

A NET\_JOIN\_RESPONSE\_PDU should be followed by N * VAL\_INSERT\_PDU (One
for each value to be transferred to the prospect).  The NEXT\_PORT is sent in
network byte order.

| Field        | Bytes |Value|
| -----        | ----- |-----|
| TYPE         | 1     |4    |
| NEXT\_ADDRESS| 4     |     |
| NEXT\_PORT   | 2     |     |
| RANGE\_START | 1     |     |
| RANGE\_END   | 1     |     |


## NET\_CLOSE\_CONNECTION\_PDU: (TCP)
Signals that the connection should be dropped.

| Field | Bytes |Value|
| ----- | ----- |-----|
| TYPE  |  1    |5    |


## NET\_NEW\_RANGE\_PDU: (TCP)
Sent from a node that is about to leave the network to one of its connected
neighbours. The RANGE\_END and RANGE\_START is initialized to the senders
range. The receiver of the NET\_NEW\_RANGE\_PDU is to expand its range
according to the RANGE\_END or RANGE\_START, depending if it is the successor
or the predecessor to the node. The PDU is to be followed by N *
VAL\_INSERT\_PDU (One for each entry in the sender node).  

| Field           | Bytes |Value|
| --------------- | ----- |-----|
| TYPE            | 1     |6    |
| RANGE\_START    | 1     |     |
| RANGE\_END      | 1     |     |


## NET\_NEW\_RANGE\_RESPONSE\_PDU: (TCP)
Sent as a response to a NET\_NEW\_RANGE to acknowledge that the node has
updated its range.

| Field           | Bytes |Value|
| --------------- | ----- |-----|
| TYPE            | 1     |8    |


## NET\_LEAVING\_PDU: (TCP)
Sent from a leaving node to its predecessor or successor. The receiver should then
drop the connection to the sender, and subsequently connecting
to the specified NEW\_\*. The NEW\_PORT is sent in network byte order.

| Field        | Bytes |Value|
| -----        | ----- |-----|
| TYPE         | 1     |7    |
| NEW\_ADDRESS | 4     |     |
| NEW\_PORT    | 2     |     |


## VAL\_INSERT\_PDU: (TCP/UDP)
Contains an entry that is to be inserted into the network. All nodes registered with the
tracker should listen for incoming VAL\_INSERT\_PDU over UDP to the listen port.
When received by a node the SSN is hashed. If the hash fits in the receiving
nodes hash range the value is inserted, else the PDU is forwarded by the node to its successor.

| Field        | Bytes        |Value|
| -----        | -----        |-----|
| TYPE         | 1            |100  |
| SSN          | 12           |     |
| NAME\_LENGTH | 1            |     |
| NAME         | NAME\_LENGTH |     |
| EMAIL\_LENGTH| 1            |     |
| EMAIL        | EMAIL\_LENGTH|     |


## VAL\_REMOVE\_PDU: (TCP/UDP)
Contains an entry that is to be removed from the network. All nodes registered with the tracker
should listen for incoming VAL\_REMOVE\_PDU over UDP to the listen port.
When received the node the SSN is hashed. If the hash fits in the receiving
nodes hash range the value is removed, else the PDU is forwarded by the node to its successor.

| Field | Bytes |Value|
| ----- | ----- |-----|
| TYPE  | 1     |101  |
| SSN   | 12    |     |


## VAL\_LOOKUP\_PDU: (TCP/UDP)
Contains a request for an entry. All nodes registered with the tracker
should listen for incoming VAL\_LOOKUP\_PDU over UDP to the listen port.
If an the SSN is contained in the receiving nodes hash-range, a
VAL\_LOOKUP\_RESPONSE is sent to the source through UDP, else the VAL\_LOOKUP\_PDU is
forwarded. The SENDER\_ID field is initialized to the senders ID. The SENDER\_PORT is
sent in network byte order.

| Field          | Bytes |Value|
| -----          | ----- |-----|
| TYPE           | 1     |102  | 
| SSN            | 12    |     | 
| SENDER\_ADDRESS| 4     |     |
| SENDER\_PORT   | 2     |     |


## VAL\_LOOKUP\_RESPONSE\_PDU: (UDP)
Sent as a response if a node can successfully match a VAL\_LOOKUP\_PDU.
The value fields are initialized from the matched data and the PDU is sent
to the source contained in the VAL\_LOOKUP\_PDU. If the entry does not exist
the pdu is to be zeroed, except for the type.

| Field         | Bytes         |Value|
| -----         | -----         |-----|
| TYPE          |  1            |103  |
| SSN           | 12            |     |
| NAME\_LENGTH  | 1             |     |
| NAME          | NAME\_LENGTH  |     |
| EMAIL\_LENGTH | 1             |     |
| EMAIL         | EMAIL\_LENGTH |     |

    
## STUN\_LOOKUP\_PDU: (UDP)
Can be sent to the tracker to discover ones public IP STUN\_LOOKUP\_PDU. The
tracker responds with a STUN\_RESPONSE\_PDU to the sender address and port. 

| Field | Bytes |Value|
| ----- | ----- |-----|
| TYPE  | 1     |200  |

    
## STUN\_RESPONSE\_PDU: (UDP)
Sent as a reponse to a STUN\_LOOKUP\_PDU. The ADDRESS field is initialized to
the source address of the STUN\_LOOKUP\_PDUs sender.

| Field   | Bytes |Value|
| -----   | ----- |-----|
| TYPE    | 1     |201  |
| ADDRESS | 4     |     |


