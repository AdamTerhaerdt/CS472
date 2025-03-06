#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>

#include "du-ftp.h"
#include "du-proto.h"


#define BUFF_SZ 4096
static char sbuffer[BUFF_SZ];
static char rbuffer[BUFF_SZ];
static char full_file_path[FNAME_SZ];

/*
 *  Helper function that processes the command line arguements.  Highlights
 *  how to use a very useful utility called getopt, where you pass it a
 *  format string and it does all of the hard work for you.  The arg
 *  string basically states this program accepts a -p or -c flag, the
 *  -p flag is for a "pong message", in other words the server echos
 *  back what the client sends, and a -c message, the -c option takes
 *  a course id, and the server looks up the course id and responds
 *  with an appropriate message. 
 */
static int initParams(int argc, char *argv[], prog_config *cfg){
    int option;
    //setup defaults if no arguements are passed
    static char cmdBuffer[64] = {0};

    //setup defaults if no arguements are passed
    cfg->prog_mode = PROG_MD_CLI;
    cfg->port_number = DEF_PORT_NO;
    strcpy(cfg->file_name, PROG_DEF_FNAME);
    strcpy(cfg->svr_ip_addr, PROG_DEF_SVR_ADDR);
    
    while ((option = getopt(argc, argv, ":p:f:a:csh")) != -1){
        switch(option) {
            case 'p':
                strncpy(cmdBuffer, optarg, sizeof(cmdBuffer));
                cfg->port_number = atoi(cmdBuffer);
                break;
            case 'f':
                strncpy(cfg->file_name, optarg, sizeof(cfg->file_name));
                break;
            case 'a':
                strncpy(cfg->svr_ip_addr, optarg, sizeof(cfg->svr_ip_addr));
                break;
            case 'c':
                cfg->prog_mode = PROG_MD_CLI;
                break;
            case 's':
                cfg->prog_mode = PROG_MD_SVR;
                break;
            case 'h':
                printf("USAGE: %s [-p port] [-f fname] [-a svr_addr] [-s] [-c] [-h]\n", argv[0]);
                printf("WHERE:\n\t[-c] runs in client mode, [-s] runs in server mode; DEFAULT= client_mode\n");
                printf("\t[-a svr_addr] specifies the servers IP address as a string; DEFAULT = %s\n", cfg->svr_ip_addr);
                printf("\t[-p portnum] specifies the port number; DEFAULT = %d\n", cfg->port_number);
                printf("\t[-f fname] specifies the filename to send or recv; DEFAULT = %s\n", cfg->file_name);
                printf("\t[-p] displays what you are looking at now - the help\n\n");
                exit(0);
            case ':':
                perror ("Option missing value");
                exit(-1);
            default:
            case '?':
                perror ("Unknown option");
                exit(-1);
        }
    }
    return cfg->prog_mode;
}

int server_loop(dp_connp dpc, void *sBuff, void *rBuff, int sbuff_sz, int rbuff_sz){
    int rcvSz;
    duftp_pdu pdu;
    FILE *f = NULL;
    bool file_opened = false;

    if (dpc->isConnected == false){
        perror("Expecting the protocol to be in connect state, but its not");
        exit(-1);
    }
    
    //Loop until a disconnect is received, or error happens
    while(1) {
        //receive request from client
        rcvSz = dprecv(dpc, rBuff, rbuff_sz);
        if (rcvSz == DP_CONNECTION_CLOSED){
            if (file_opened) {
                fclose(f);
            }
            printf("Client closed connection\n");
            return DP_CONNECTION_CLOSED;
        }
        
        // Process based on PDU message type
        memcpy(&pdu, rBuff, sizeof(duftp_pdu));
        
        switch(pdu.msg_type) {
            case DUFTP_MSG_FILENAME:
                // Client is sending a filename
                strncpy(full_file_path, "./infile/", FNAME_SZ);
                strncat(full_file_path, pdu.filename, FNAME_SZ - strlen(full_file_path) - 1);
                
                f = fopen(full_file_path, "wb+");
                if(f == NULL){
                    printf("ERROR: Cannot open file %s\n", full_file_path);
                    // Send error response
                    pdu.msg_type = DUFTP_MSG_ERROR;
                    pdu.error_code = DUFTP_ERR_FILE_NOT_FOUND;
                    dpsend(dpc, &pdu, sizeof(duftp_pdu));
                } else {
                    file_opened = true;
                    // Send acknowledgment
                    pdu.msg_type = DUFTP_MSG_FILENAME;
                    pdu.error_code = DUFTP_ERR_NONE;
                    dpsend(dpc, &pdu, sizeof(duftp_pdu));
                }
                printf("Received filename: %s\n", pdu.filename);
                break;
                
            case DUFTP_MSG_DATA:
                if (!file_opened) {
                    printf("ERROR: Received data before file was opened\n");
                    pdu.msg_type = DUFTP_MSG_ERROR;
                    pdu.error_code = DUFTP_ERR_UNKNOWN;
                    dpsend(dpc, &pdu, sizeof(duftp_pdu));
                    continue;
                }
                
                // Write data to file
                fwrite((char*)rBuff + sizeof(duftp_pdu), 1, pdu.data_size, f);
                printf("Received %d bytes of data\n", pdu.data_size);
                break;
                
            case DUFTP_MSG_COMPLETE:
                printf("File transfer complete\n");
                if (file_opened) {
                    fclose(f);
                    file_opened = false;
                }
                // Send acknowledgment
                pdu.msg_type = DUFTP_MSG_COMPLETE;
                pdu.error_code = DUFTP_ERR_NONE;
                dpsend(dpc, &pdu, sizeof(duftp_pdu));
                break;
                
            case DUFTP_MSG_ERROR:
                printf("Received error from client: %d\n", pdu.error_code);
                break;
                
            default:
                printf("Unknown message type: %d\n", pdu.msg_type);
                break;
        }
    }
}

void start_client(dp_connp dpc){
    duftp_pdu pdu;
    char data_buffer[BUFF_SZ - sizeof(duftp_pdu)];
    int bytes_read;

    if(!dpc->isConnected) {
        printf("Client not connected\n");
        return;
    }

    FILE *f = fopen(full_file_path, "rb");
    if(f == NULL){
        printf("ERROR: Cannot open file %s\n", full_file_path);
        exit(-1);
    }
    
    // First, send the filename
    memset(&pdu, 0, sizeof(duftp_pdu));
    pdu.msg_type = DUFTP_MSG_FILENAME;
    pdu.error_code = DUFTP_ERR_NONE;
    strncpy(pdu.filename, strrchr(full_file_path, '/') + 1, FNAME_SZ - 1);
    
    dpsend(dpc, &pdu, sizeof(duftp_pdu));
    
    // Wait for server acknowledgment
    dprecv(dpc, &pdu, sizeof(duftp_pdu));
    if (pdu.msg_type == DUFTP_MSG_ERROR) {
        printf("Server reported error: %d\n", pdu.error_code);
        fclose(f);
        dpdisconnect(dpc);
        return;
    }
    
    // Send file data in chunks
    while ((bytes_read = fread(data_buffer, 1, sizeof(data_buffer), f)) > 0) {
        pdu.msg_type = DUFTP_MSG_DATA;
        pdu.data_size = bytes_read;
        
        // Copy PDU header to send buffer
        memcpy(sbuffer, &pdu, sizeof(duftp_pdu));
        // Copy file data after PDU header
        memcpy(sbuffer + sizeof(duftp_pdu), data_buffer, bytes_read);
        
        // Send combined PDU header and data
        dpsend(dpc, sbuffer, sizeof(duftp_pdu) + bytes_read);
    }
    
    // Send completion message
    pdu.msg_type = DUFTP_MSG_COMPLETE;
    pdu.data_size = 0;
    dpsend(dpc, &pdu, sizeof(duftp_pdu));
    
    // Wait for server acknowledgment
    dprecv(dpc, &pdu, sizeof(duftp_pdu));
    
    fclose(f);
    dpdisconnect(dpc);
}

void start_server(dp_connp dpc){
    server_loop(dpc, sbuffer, rbuffer, sizeof(sbuffer), sizeof(rbuffer));
}


int main(int argc, char *argv[])
{
    prog_config cfg;
    int cmd;
    dp_connp dpc;
    int rc;


    //Process the parameters and init the header - look at the helpers
    //in the cs472-pproto.c file
    cmd = initParams(argc, argv, &cfg);

    printf("MODE %d\n", cfg.prog_mode);
    printf("PORT %d\n", cfg.port_number);
    printf("FILE NAME: %s\n", cfg.file_name);

    switch(cmd){
        case PROG_MD_CLI:
            //by default client will look for files in the ./outfile directory
            snprintf(full_file_path, sizeof(full_file_path), "./outfile/%s", cfg.file_name);
            dpc = dpClientInit(cfg.svr_ip_addr,cfg.port_number);
            rc = dpconnect(dpc);
            if (rc < 0) {
                perror("Error establishing connection");
                exit(-1);
            }

            start_client(dpc);
            exit(0);
            break;

        case PROG_MD_SVR:
            //by default server will look for files in the ./infile directory
            snprintf(full_file_path, sizeof(full_file_path), "./infile/%s", cfg.file_name);
            dpc = dpServerInit(cfg.port_number);
            rc = dplisten(dpc);
            if (rc < 0) {
                perror("Error establishing connection");
                exit(-1);
            }

            start_server(dpc);
            break;
        default:
            printf("ERROR: Unknown Program Mode.  Mode set is %d\n", cmd);
            break;
    }
}