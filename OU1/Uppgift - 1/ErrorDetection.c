/*
1.1 Description: simple error detection
You receive the following message row by row:
01001001 00100000 01100011 01101101
00100000 01001011 01101000 01101110
01100011 01110101 01100110 01100111
Verify whether the message is garbled based on the information provided below:
1. Two-dimensional parity: Dividing the message into 16 bit rows, the
even row parity reads:
0
0
1
0
1
1
and the even column parity reads:
0110 0101 0111 1010.
Assume that all error detection bits are uncorrupted.

*/