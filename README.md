
# IRC Connect

Basic IRC connection and client on Linux/Unix using BSD sockets

 - Receives 512 bytes at a time, splitting into lines delimited by CR-LF
 - If we have a partial line at the end of recv(), this is kept in memory to prefix to the next recv()
 - Responds automatically to PING requests to keep connection alive as per RFC 2812
