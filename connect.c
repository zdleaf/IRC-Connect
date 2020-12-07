/*
    v0.4
    IRC Connect - zinc.london
    https://github.com/zdleaf
    basic IRC connection and client using BSD sockets
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAXDATASIZE 512 // max bytes in each call to recv() + size of malloc'd buffer
#define NICKNAME "foobar"
#define IDENT "foobar"
#define REALNAME "foobar"
#define JOIN "join #98755843"

// recv() data into a buffer through a BSD socket and process this
int recv_buf(int *socket, char *buffer, int *overflow, char *temp, int temp_len);
int proc_buf(char *buffer, int *socket);
int chk_overflow(char *buffer); 
void proc_overflow(char *buffer, char *temp);

// IRC functions
void send_info(int *socket);
void send_ping(int *socket, int size, char *buf);
void join_chan(int *socket);

int main(int argc, char *argv[]){
    
    struct hostent *he;
    struct sockaddr_in their_addr;
    
    if (argc != 3){				// Usage conditions.
        fprintf(stderr, "usage: %s hostname port\n", argv[0]);
        exit(1);
    }
    
    /* arg 2 : PORT NUMBER */
    int arg2_len;
    unsigned long int port;
    arg2_len = strlen(argv[2]);			// add one byte for NULL terminator to make string
    char *portbuf;
    portbuf = (char *)malloc(arg2_len);	// allocate memory for buffer2
    if(portbuf == NULL) {				// malloc() error checking
        printf("Error in memory allocation (malloc)\n");
        exit(1);
    }
    strcpy(portbuf, argv[2]);			// copy arg into buffer
    port = strtol(portbuf, NULL, 10);	// convert it to an unsigned long int
    if(port < 1 || port > 65535) {		// check it's a valid port number
        printf("The number you entered is not a valid port number\n");
        exit(1);
    } 
    /* END */
    
    int sockfd;
    
    if ((he=gethostbyname(argv[1])) == NULL){	// get host info
        herror("gethostbyname");				// return error on failure & quit
        exit(1);
    }
    
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1){ 	// create socket 'sockfd'
        perror("socket");									// return error on failure & quit
        exit(1);
    }
    
    their_addr.sin_family = AF_INET; 		// host byte order
    their_addr.sin_port = htons(port);		// short to network byte order
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(their_addr.sin_zero), '\0', 8);// zero the rest of the struct		
    
    if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1){	// connect to socket sockfd
        perror("connect");
        exit(1);
    }
    
    printf("Connecting to %s on port %d\n", argv[1], port);		// send connection message
    
    char *buffer, *temp;							// allocate and initialise buffers for recv'd data
    buffer = calloc(MAXDATASIZE, sizeof(char));		// allocate 512 bytes for each (MAXDATASIZE)
    if(buffer == NULL) {							// malloc() error checking
        printf("Error in memory allocation (malloc)\n");
        exit(1);
    }
    temp = calloc(MAXDATASIZE, sizeof(char));		// 'temp' and 'buffer' are pointers to first byte of allocated memory
    if(temp == NULL) {								// malloc() error checking
        printf("Error in memory allocation (malloc)\n");
        exit(1);
    }
    
    int overflow, i;
    overflow = 0;							        // initially set overflow to 0 (false) for first recv() on first run of loop
    
    while (recv_buf(&sockfd, buffer, &overflow, temp, strlen(temp)) != 0){

        overflow = chk_overflow(buffer);		        // check for overflow and set flag accordingly
    
        if(overflow){					        // if there is an overflow
            for(i = 0; i <= strlen(temp); i++) {	// zero the temporary buffer ready for new data
                temp[i]='\0';
            }
            proc_overflow(buffer, temp);		    // process the overflow and place into temp
        }
    
        proc_buf(buffer, &sockfd);		            // process the buffer	
        for(i = 0; i <= MAXDATASIZE - 1; i++) {	    // zero the buffer ready for new data
            buffer[i]='\0';
        }
    } 
    printf("Error in recv_buf()\n");	
}

int recv_buf(int *socket, char *buffer, int *overflow, char *temp, int temp_len){
    
    /**
    * recv_buf()
    * recieves data from socket into malloc'd buffer.
    * if there's been an overflow, recv_buf() gets overflowed string from temp (from proc_overflow()) and concatenates it w/ new recv.
    * maintains 512 bytes as max buffer size.
    **/
    
    int numbytes;
    if(!*overflow){										// if there is no overflow
        numbytes = recv(*socket, buffer, MAXDATASIZE, 0);	// receive through socket into buffer
        if(numbytes == -1) {								// recv() error checking
            perror("recv, overflow = 0");
            return 0;
        }
        if(numbytes == 0) {
            printf("Host closed connection.");
            return 0;
        }
        return 1;

    }
    else if(*overflow){
        
        //printf("***OVERWRITE FLAG = 1 - overflow=[%s]\n", temp);
        
        int i;
        char *buffer2;
        
        buffer2 = malloc(MAXDATASIZE - temp_len);	// allocate 512-temp_len bytes for extra data
        if(buffer == NULL) {						// malloc() error checking
            printf("Error in memory allocation (malloc)\n");
            return 0;
        }
        numbytes = recv(*socket, buffer2, MAXDATASIZE - temp_len, 0);	// receive through socket into 2nd buffer
        if(numbytes == -1) {											// recv() error checking
            perror("recv, overflow = 1");
            return 0;
        }
        strncpy(buffer, temp, temp_len);					// copy temp(overflow) into buffer
        strncat(buffer, buffer2, strlen(buffer2));			// concatenate new recv() into buffer
        
        for(i = 0; i <= temp_len; i++) {					// zero the temporary buffer ready for new data
            temp[i]='\0';
        }
        
        return 1;
    }
}

int proc_buf(char *buffer, int *socket){
    
    /**
    * proc_buf()
    * using token() splits a given buffer one-by-one into strings delimited by the CR-LF('\n') sequence
    * these strings are then compared with commands and the appropriate cmd_functions are run
    * note: the below string comparison else-if statements are temporary, better pattern matching is required for different servers as they return different strings
    **/
    
    char *token;
    char *delimiter = "\n";	
    
    if ((token = strtok(buffer, delimiter)) != NULL) {	// split everything into buffers delimited by \n characters
        do {
            int token_len;
            token_len = strlen(token);
            token[token_len - 1] = '\n';
            
            printf("%s", token);
        
            int result_nick, result_info;
            result_nick = 1;
            char *noticeauth = "NOTICE AUTH :*** No";
            char *ping = "PING :";
            char *pong;
            char *error = "ERROR :";
            char *motd = "End of /MOTD command.";
            if (strncmp(token, noticeauth, strlen(noticeauth)) == 0){	// if a notice auth, print it
                send_info((int *)socket);
            } else if (strncmp(token, error, strlen(error)) == 0){		// if error, exit
                printf("Error: %s\n", token);
                exit(1);
            } else if ((strncmp(token, ping, strlen(ping))) == 0){		// if ping, then reply
                send_ping((int *)socket, strlen(token), token);
            } else if ((strstr(token, motd)) != NULL){					// if end of the motd, join the chan specified
                join_chan((int *)socket);
            } 
        } 
        while ((token = strtok(NULL, delimiter)) != NULL);
    }
return 0;	
}

int chk_overflow(char *buffer){
    
    /**
    * chk_overflow()
    * simple function to check if the buffer contains an overflow of text or not
    **/	

    char *last;
    last = strrchr(buffer, '\n');		// last is the last instance of \n in the recv()'d string
    if (last == NULL){
        printf("Error in chk_overflow. Unable to find CR-LF char in string.\n");
        exit(1);
    }
    if (*(last + 1) != '\0') {			// if last char after the CR-LF is not a string NUL terminator, it's overflowing
        return 1;						// return 1, there is an overflow
    }
    return 0; 						    // else there is no overflow, send straight to proc_buffer()
}
    
void proc_overflow(char *buffer, char *temp){
    
    /**
    * proc_overflow()
    * given a buffer with overflowing text (does not end with CR-LF), proc_overflow() strips off the overflow and puts it into temp
    * the buffer can then be processed via proc_buffer, and recv_buf(overflow=1) can recv() on top of overflow
    **/
    
    char *last;
    last = strrchr(buffer, '\n') + 1;	// last points to the char 1 right of last instance of \n in the recv()'d string
    strncpy(temp, last, strlen(last));	// copy the overflow into temp
    *last = '\0';						// truncate buffer (cut off overflow) by adding a NUL terminator
    
}

void send_info(int *socket) {
    int bytes_sent;
    char *nick = "nick "NICKNAME" \n";
    char *user = "user "IDENT" 8 * : "REALNAME" \n";
    if ((bytes_sent = send(*socket, nick, (strlen(nick)), 0)) == -1) {
        perror("send nick");
        exit(1);
    }
    printf("%d bytes sent for nickname\n", bytes_sent);
    
    if ((bytes_sent = send(*socket, user, (strlen(user)), 0)) == -1) {
        perror("send user info");
        exit(1);
    }
    printf("%d bytes sent for user details\n", bytes_sent);
}

void send_ping(int *socket, int size, char *buf){
    int bytes_sent, i;
    char pingbuf[size];
    for (i = 0; i < size; i++){		// copy value of buffer into pingbuf
        pingbuf[i] = buf[i];
    }
    pingbuf[1] = 'O';				// change 2nd byte of PING to 'O' (to make PONG)
    pingbuf[size] = '\0';			// terminate string with null char
    printf("Sending ping reply: %s", pingbuf);
    if ((bytes_sent = send(*socket, pingbuf, (strlen(pingbuf)), 0)) == -1){	// send back modified pingbuf
        perror("send");
        exit(1);
    }
}

void join_chan(int *socket){
    int bytes_sent;
    char *join = ""JOIN" \n";
    if ((bytes_sent = send(*socket, join, (strlen(join)), 0)) == -1){	// send join string
        perror("send");
        exit(1);
    }
}