#include "http.h"

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define  BUFF_SZ            1024
#define  MAX_REOPEN_TRIES   5

char recv_buff[BUFF_SZ];

char *generate_cc_request(const char *host, int port, const char *path){
	static char req[512] = {0};
	int offset = 0;
	
    //note that all paths should start with "/" when passed in
	offset += sprintf((char *)(req + offset),"GET %s HTTP/1.1\r\n", path);
	offset += sprintf((char *)(req + offset),"Host: %s\r\n", host);
	offset += sprintf((char *)(req + offset),"Connection: Keep-Alive\r\n");
	offset += sprintf((char *)(req + offset),"\r\n");

	printf("DEBUG: %s", req);
	return req;
}


void print_usage(char *exe_name){
    fprintf(stderr, "Usage: %s <hostname> <port> <path...>\n", exe_name);
    fprintf(stderr, "Using default host %s, port %d  and path [\\]\n", DEFAULT_HOST, DEFAULT_PORT); 
}

int reopen_socket(const char *host, uint16_t port) {
    int sock = 0;

    //Attempting to connect to the server MAX_REOPEN_TRIES times
    for (int i = 0; i < MAX_REOPEN_TRIES; i++) {
        sock = socket_connect(host, port);
        if (sock >= 0) {
            return sock;
        }
    }
    
    //Returning -1 if the socket failed to connect
    return -1;
}

int server_connect(const char *host, uint16_t port){
    return socket_connect(host, port);
}

void server_disconnect(int sock){
    close(sock);
}

int submit_request(int sock, const char *host, uint16_t port, char *resource){
    int sent_bytes = 0; 

    const char *req = generate_cc_request(host, port, resource);
    int send_sz = strlen(req);

    // This is the initial send, this is where the send will fail with 
    // Keep-Alive if the server closed the socket, sent_bytes will have
    // the number of bytes sent, which should equal send_sz if all went
    // well.  If sent_bytes < 0, this indicates a send error, so we will
    // need to reconnect to the server.
    sent_bytes = send(sock, req, send_sz,0);

    //we have a socket error, perhaps the server closed it, lets try to reopen
    //the socket
    if (sent_bytes < 0){
        //Reopen the socket
        sock = reopen_socket(host, port);
        if (sock < 0) {
            return sock;
        }

        //Reissue the send with new socket
        sent_bytes = send(sock, req, send_sz, 0);
    }

    //This should not happen, but just checking if we didnt send everything and 
    //handling appropriately 
    if(sent_bytes != send_sz){
        if(sent_bytes < 0)
            perror("send failed after reconnect attempt");
        else
            fprintf(stderr, "Sent bytes %d is not equal to sent size %d\n", sent_bytes, send_sz);
        
        close(sock);
        return -1;
    }

    int bytes_recvd = 0;    //used to track amount of data received on each recv() call
    int total_bytes = 0;    //used to accumulate the total number of bytes across all recv() calls
    
    //do the first recv
    bytes_recvd = recv(sock, recv_buff, sizeof(recv_buff),0);
    if(bytes_recvd < 0) {
        perror("initial receive failed");
        close(sock);
        return -1;
    }

    //remember the first receive we just did has the HTTP header, and likely some body
    //data.  We need to determine how much data we expect

    //Get the header length from the received data
    int header_len = get_http_header_len(recv_buff, bytes_recvd);
    if(header_len < 0) {
        close(sock);
        return -1;
    }
    

    //--------------------------------------------------------------------------------
    //TODO:  Get the conetent len
    //
    // 1. Use the get_http_content_len() function to set the content_len variable.
    //
    // Note that no error checking is needed, if the get_http_content_len() function
    // cannot find a Content-Length header, its assumed as per the HTTP spec that ther
    // is no body, AKA, content_len is zero;
    //--------------------------------------------------------------------------------
    int content_len = get_http_content_len(recv_buff, bytes_recvd);

    //--------------------------------------------------------------------------------
    // TODO:  Make sure you understand the calculations below
    //
    // You do not have to write any code, but add to this comment your thoughts on 
    // what the following 2 lines of code do to track the amount of data received
    // from the server
    //
    // YOUR ANSWER:  On line 144, the initial_data variable is calculated by 
    // subtracting the header_len from the bytes_recvd. This is because we want to
    // determine the amount of data in that is in the HTTP body. We do this by
    // subtracting the header_len from the bytes_recvd. Bytes_recvd is the total
    // number of bytes received from the server, and the header_len is the length
    // of the HTTP header. The bytes_remaining variable is then calculated by taking
    // the inital_data variable we just created and subtracting it from the content_len.
    // This variable shows how many bytes remaining there are to be received from the server.
    //--------------------------------------------------------------------------------
    int initial_data =  bytes_recvd - header_len;
    int bytes_remaining = content_len - initial_data;


    //This loop keeps going until bytes_remaining is essentially zero, to be more
    //defensive an prevent an infinite loop, i have it set to keep looping as long
    //as bytes_remaining is positive
    while(bytes_remaining > 0){
        //Make recv() call to get more data from server
        bytes_recvd = recv(sock, recv_buff, sizeof(recv_buff), 0);
        
        //Check for receive error
        if(bytes_recvd < 0) {
            perror("receive failed");
            close(sock);
            return -1;
        }
        
        //Print received data and update counters
        fprintf(stdout, "%.*s", bytes_recvd, recv_buff);
        total_bytes += bytes_recvd;
        fprintf(stdout, "remaining %d, received %d\n", bytes_remaining, bytes_recvd);
        bytes_remaining -= bytes_recvd;
    }

    fprintf(stdout, "\n\nOK\n");
    fprintf(stdout, "TOTAL BYTES: %d\n", total_bytes);

    //processed the request OK, return the socket, in case we had to reopen
    //so that it can be used in the next request

    //---------------------------------------------------------------------------------
    // TODO:  Documentation
    //
    // You dont have any code to change, but explain why this function, if it gets to this
    // point returns an active socket.
    //
    // YOUR ANSWER: The reason why we are returning an active socket is because up until
    // this point there have been no errors that would have closed the socket. We have
    // reached the end of the function and we return the active socket because it allows
    // the caller of this function to reuse the same socket for other requests it may
    // decide to make. This helps with not having to repeatedly open and close connections.
    //--------------------------------------------------------------------------------
    return sock;
}


//This main function is the entry point of this program and it handles
//the command line arguments, initializes variables, creates and disconnects the server connection,
//and submits requests.
int main(int argc, char *argv[]){
    clock_t start_time = clock();

    int sock;

    const char *host = DEFAULT_HOST;
    uint16_t   port = DEFAULT_PORT;
    char       *resource = DEFAULT_PATH;
    int        remaining_args = 0;

    //YOU DONT NEED TO DO ANYTHING OR MODIFY ANYTHING IN MAIN().  MAKE SURE YOU UNDERSTAND
    //THE CODE HOWEVER
    sock = server_connect(host, port);

    if(argc < 4){
        print_usage(argv[0]);
        //process the default request
        submit_request(sock, host, port, resource);
	} else {
        host = argv[1];
        port = atoi(argv[2]);
        resource = argv[3];
        if (port == 0) {
            fprintf(stderr, "NOTE: <port> must be an integer, using default port %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
        fprintf(stdout, "Running with host = %s, port = %d\n", host, port);
        remaining_args = argc-3;
        for(int i = 0; i < remaining_args; i++){
            resource = argv[3+i];
            //fprintf(stdout, "\n\nProcessing request for %s\n\n", resource);
            sock = submit_request(sock, host, port, resource);
        }
    }

    server_disconnect(sock);

    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Total runtime: %.6f seconds\n", elapsed_time);

    return 0;
}