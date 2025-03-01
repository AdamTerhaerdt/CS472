#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include "http.h"

//---------------------------------------------------------------------------------
// TODO:  Documentation
//
// Note that this module includes a number of helper functions to support this
// assignment.  YOU DO NOT NEED TO MODIFY ANY OF THIS CODE.  What you need to do
// is to appropriately document the socket_connect(), get_http_header_len(), and
// get_http_content_len() functions. 
//
// NOTE:  I am not looking for a line-by-line set of comments.  I am looking for 
//        a comment block at the top of each function that clearly highlights you
//        understanding about how the function works and that you researched the
//        function calls that I used.  You may (and likely should) add additional
//        comments within the function body itself highlighting key aspects of 
//        what is going on.
//
// There is also an optional extra credit activity at the end of this function. If
// you partake, you need to rewrite the body of this function with a more optimal 
// implementation. See the directions for this if you want to take on the extra
// credit. 
//--------------------------------------------------------------------------------

//The README.MD said we only need to document socket_connect, get_http_header_len, and get_http_content_len.
//I am not documenting these functions as per directions.
char *strcasestr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		c = tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while ((char)tolower((unsigned char)sc) != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

//The README.MD said we only need to document socket_connect, get_http_header_len, and get_http_content_len.
//I am not documenting these functions as per directions.
char *strnstr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == '\0' || slen-- < 1)
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

//DOCUMENTATION:
//This function is used to establish a TCP connection for a given host and a port.
//The main purpose is to first identify a host name and establish an IP address, if
//that fails then the funciton prints an `herror()` and returns -2. Then if the function
//successfully finds a host it will copy the contents of the host into the `addr` 
//variable. It sets the port number converting it from host to network and then creates
//a socket. If there is an error with creating the socket it reutnr -1 along with `perror`.
//Finally, the function attempts to establish a connection to the server.
int socket_connect(const char *host, uint16_t port){
    struct hostent *hp;
    struct sockaddr_in addr;
    int sock;

    //Attempts to get host information
    if((hp = gethostbyname(host)) == NULL){
		herror("gethostbyname");
		return -2;
	}
    
    //Copy's host information to the addr variable
	bcopy(hp->h_addr_list[0], &addr.sin_addr, hp->h_length);

    //Converts port from host to network
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;

    //Opens socket
	sock = socket(PF_INET, SOCK_STREAM, 0); 
	
    //If problem with socket returns error
	if(sock == -1){
		perror("socket");
		return -1;
	}

    //Attempts to establish connection
    if(connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
		perror("connect");
		close(sock);
        return -1;
	}

    return sock;
}


//DOCUMENTATION:
//This function is used to determine the length of a header given a buffer.
//http_buff is a pointer to the http data, and http_buff_len the length of
//the buffer. End_ptr finds the end of the http header within the buffer. If
//it cannot find the end of the header (end_ptr == NULL), then it throws an error.
//If it can find the end of the header, it will then calculate the length of the header
//by subtracting the end_ptr by the http_buff and adding the length of the HTTP_HEADER_END.
//Finally this function will return the header length.
int get_http_header_len(char *http_buff, int http_buff_len){
    char *end_ptr;
    int header_len = 0;
    //Trys to find the first occurance of HTTP_HEADER_END
    end_ptr = strnstr(http_buff,HTTP_HEADER_END,http_buff_len);

    //If it can't, through an error
    if (end_ptr == NULL) {
        fprintf(stderr, "Could not find the end of the HTTP header\n");
        return -1;
    }

    //Calculate the header length
    header_len = (end_ptr - http_buff) + strlen(HTTP_HEADER_END);

    return header_len;
}


//DOCUMENTATION:
//This function is used to the get the content length of a http_header with a
//given buffer. The http_buff is a pointer to the HTTP response header, and the
//http_header_len is the length of the HTTP header within the buffer. The variable
//header_line is used to store each line of the http headers as they are porsed, 
//next_header_line is to store a pointer to current position of the buffer, and
//end_header_buff is a pointer to the end of the headers in the buffer. The function
//loops through each line of the headers until it has reached the end of the headers.
//`bzero()` sets the header_line to null bytes, essentially clears it, and then scanf
//reads the next line. isCLHeader and isCLHeader2 checks if the current line contains
//CL_HEADER, if it does not then it finds the delimiter, and then converts the header
//value from a string to an integer using `atoi`. If the content header is found,
//then the function returns the content length, if it is not found then it prints
//an error and returns 0.
int get_http_content_len(char *http_buff, int http_header_len){
    //Store each line of the http header
    char header_line[MAX_HEADER_LINE];

    char *next_header_line = http_buff;
    char *end_header_buff = http_buff + http_header_len;

    //Loop to interate through each header until it is at the end of the headers
    while (next_header_line < end_header_buff){

        //Clear the header
        bzero(header_line,sizeof(header_line));

        //Scans in the next header
        sscanf(next_header_line,"%[^\r\n]s", header_line);

        //Checks if they are the CL_HEADER (ignoring case)
        char *isCLHeader2 = strcasecmp(header_line,CL_HEADER);
        char *isCLHeader = strcasestr(header_line,CL_HEADER);
        if(isCLHeader != NULL){
            //Parses the header_line using a delimiter
            char *header_value_start = strchr(header_line, HTTP_HEADER_DELIM);
            if (header_value_start != NULL){
                //Found header, convert from string to int and return
                char *header_value = header_value_start + 1;
                int content_len = atoi(header_value);
                return content_len;
            }
        }
        next_header_line += strlen(header_line) + strlen(HTTP_HEADER_EOL);
    }
    //print error and return 0
    fprintf(stderr,"Did not find content length\n");
    return 0;
}

//This function just prints the header, it might be helpful for your debugging
//You dont need to document this or do anything with it, its self explanitory. :-)
void print_header(char *http_buff, int http_header_len){
    fprintf(stdout, "%.*s\n",http_header_len,http_buff);
}

//--------------------------------------------------------------------------------------
//EXTRA CREDIT - 10 pts - READ BELOW
//
// Implement a function that processes the header in one pass to figure out BOTH the
// header length and the content length.  I provided an implementation below just to 
// highlight what I DONT WANT, in that we are making 2 passes over the buffer to determine
// the header and content length.
//
// To get extra credit, you must process the buffer ONCE getting both the header and content
// length.  Note that you are also free to change the function signature, or use the one I have
// that is passing both of the values back via pointers.  If you change the interface dont forget
// to change the signature in the http.h header file :-).  You also need to update client-ka.c to 
// use this function to get full extra credit. 
//--------------------------------------------------------------------------------------
int process_http_header(char *http_buff, int http_buff_len, int *header_len, int *content_len){
    int h_len, c_len = 0;
    h_len = get_http_header_len(http_buff, http_buff_len);
    if (h_len < 0) {
        *header_len = 0;
        *content_len = 0;
        return -1;
    }
    c_len = get_http_content_len(http_buff, http_buff_len);
    if (c_len < 0) {
        *header_len = 0;
        *content_len = 0;
        return -1;
    }

    *header_len = h_len;
    *content_len = c_len;
    return 0; //success
}