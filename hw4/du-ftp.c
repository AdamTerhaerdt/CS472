#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/stat.h>

#include "du-ftp.h"
#include "du-proto.h"

#define BUFF_SZ DUFTP_MAX_DATA_SIZE
static char sbuffer[BUFF_SZ];
static char rbuffer[BUFF_SZ];
static char full_file_path[FNAME_SZ];
static int sequence_number = 0;

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
    duftp_pdu recv_pdu;
    duftp_pdu send_pdu;
    FILE *f = NULL;
    char output_filename[FNAME_SZ];
    int total_bytes_received = 0;
    int expected_seq_num = 0;

    if (dpc->isConnected == false){
        perror("Expecting the protocol to be in connect state, but its not");
        exit(-1);
    }

    sequence_number = 0;
    
    printf("Server waiting for file transfer...\n");
    
    //Loop until a disconnect is received, or error happens
    while(1) {
        //Receive PDU from client
        rcvSz = dprecv(dpc, &recv_pdu, sizeof(duftp_pdu));
        if (rcvSz == DP_CONNECTION_CLOSED){
            if (f != NULL) {
                fclose(f);
            }
            printf("Client closed connection\n");
            return DP_CONNECTION_CLOSED;
        }
        
        //Check sequence number
        if (recv_pdu.seq_num != expected_seq_num) {
            printf("Warning: Received out-of-sequence packet. Expected %d, got %d\n", 
                   expected_seq_num, recv_pdu.seq_num);
        }
        expected_seq_num = recv_pdu.seq_num + 1;
        
        //Process based on message type
        switch (recv_pdu.msg_type) {
            case DUFTP_MSG_FILENAME:
                printf("Receiving file: %s (size: %d bytes)\n", 
                       recv_pdu.filename, recv_pdu.total_size);
                snprintf(output_filename, sizeof(output_filename), "./infile/%s", recv_pdu.filename);
                f = fopen(output_filename, "wb+");
                if (f == NULL) {
                    printf("ERROR: Cannot open file %s for writing\n", output_filename);
                    memset(&send_pdu, 0, sizeof(send_pdu));
                    send_pdu.msg_type = DUFTP_MSG_ERROR;
                    send_pdu.protocol_ver = DUFTP_PROTOCOL_VER;
                    send_pdu.seq_num = sequence_number++;
                    send_pdu.error_code = DUFTP_ERR_FILE_NOT_FOUND;  
                    dpsend(dpc, &send_pdu, sizeof(duftp_pdu));
                    return -1;
                }
                
                //Send acknowledgment
                memset(&send_pdu, 0, sizeof(send_pdu));
                send_pdu.msg_type = DUFTP_MSG_ACK;
                send_pdu.protocol_ver = DUFTP_PROTOCOL_VER;
                send_pdu.seq_num = sequence_number++;
                
                dpsend(dpc, &send_pdu, sizeof(duftp_pdu));
                break;
                
            case DUFTP_MSG_DATA:
                if (f == NULL) {
                    printf("ERROR: Received data without filename\n");
                    memset(&send_pdu, 0, sizeof(send_pdu));
                    send_pdu.msg_type = DUFTP_MSG_ERROR;
                    send_pdu.protocol_ver = DUFTP_PROTOCOL_VER;
                    send_pdu.seq_num = sequence_number++;
                    send_pdu.error_code = DUFTP_ERR_UNKNOWN;
                    
                    dpsend(dpc, &send_pdu, sizeof(duftp_pdu));
                    return -1;
                }
                fwrite(recv_pdu.data, 1, recv_pdu.data_size, f);
                total_bytes_received += recv_pdu.data_size;
                
                printf("Received %d bytes (total: %d)\n", 
                       recv_pdu.data_size, total_bytes_received);
                
                //Send acknowledgment
                memset(&send_pdu, 0, sizeof(send_pdu));
                send_pdu.msg_type = DUFTP_MSG_ACK;
                send_pdu.protocol_ver = DUFTP_PROTOCOL_VER;
                send_pdu.seq_num = sequence_number++;
                
                dpsend(dpc, &send_pdu, sizeof(duftp_pdu));
                break;
                
            case DUFTP_MSG_COMPLETE:
                //File transfer complete
                printf("File transfer complete: %d bytes received\n", total_bytes_received);
                
                if (f != NULL) {
                    fclose(f);
                    f = NULL;
                }
                memset(&send_pdu, 0, sizeof(send_pdu));
                send_pdu.msg_type = DUFTP_MSG_ACK;
                send_pdu.protocol_ver = DUFTP_PROTOCOL_VER;
                send_pdu.seq_num = sequence_number++;
                
                dpsend(dpc, &send_pdu, sizeof(duftp_pdu));
                printf("Waiting for client to disconnect...\n");
                return 0;
                
            case DUFTP_MSG_ERROR:
                printf("Error from client: %d\n", recv_pdu.error_code);
                
                if (f != NULL) {
                    fclose(f);
                    f = NULL;
                }
                break;
                
            default:
                printf("Unknown message type: %d\n", recv_pdu.msg_type);
                break;
        }
    }
}

void start_client(dp_connp dpc){
    duftp_pdu send_pdu;
    duftp_pdu recv_pdu;
    struct stat file_stat;
    int bytes_read = 0;
    int total_bytes_sent = 0;
    FILE *f;
    int rcvSz;

    if(!dpc->isConnected) {
        printf("Client not connected\n");
        return;
    }

    f = fopen(full_file_path, "rb");
    if(f == NULL){
        printf("ERROR: Cannot open file %s\n", full_file_path);
        exit(-1);
    }
    stat(full_file_path, &file_stat);
    long file_size = file_stat.st_size;

    char *filename = strrchr(full_file_path, '/');
    if (filename == NULL) {
        filename = full_file_path;
    } else {
        filename++; 
    }
    
    sequence_number = 0;

    memset(&send_pdu, 0, sizeof(send_pdu));
    send_pdu.msg_type = DUFTP_MSG_FILENAME;
    send_pdu.protocol_ver = DUFTP_PROTOCOL_VER;
    send_pdu.seq_num = sequence_number++;
    send_pdu.data_size = 0;
    send_pdu.total_size = file_size;
    strncpy(send_pdu.filename, filename, FNAME_SZ - 1);
    
    printf("Sending file: %s (size: %ld bytes)\n", filename, file_size);
    
    dpsend(dpc, &send_pdu, sizeof(duftp_pdu));

    rcvSz = dprecv(dpc, &recv_pdu, sizeof(duftp_pdu));
    if (rcvSz == DP_CONNECTION_CLOSED) {
        printf("Server closed connection\n");
        fclose(f);
        return;
    }
    
    if (recv_pdu.msg_type == DUFTP_MSG_ERROR) {
        printf("Error from server: %d\n", recv_pdu.error_code);
        fclose(f);
        return;
    }
    
    if (recv_pdu.msg_type != DUFTP_MSG_ACK) {
        printf("Unexpected response from server: %d\n", recv_pdu.msg_type);
        fclose(f);
        return;
    }
    
    while ((bytes_read = fread(send_pdu.data, 1, DUFTP_MAX_DATA_SIZE, f)) > 0) {
        send_pdu.msg_type = DUFTP_MSG_DATA;
        send_pdu.protocol_ver = DUFTP_PROTOCOL_VER;
        send_pdu.seq_num = sequence_number++;
        send_pdu.data_size = bytes_read;
        
        dpsend(dpc, &send_pdu, sizeof(duftp_pdu));
        total_bytes_sent += bytes_read;
        
        printf("Sent %d bytes (total: %d/%ld)\n", bytes_read, total_bytes_sent, file_size);

        rcvSz = dprecv(dpc, &recv_pdu, sizeof(duftp_pdu));
        if (rcvSz == DP_CONNECTION_CLOSED) {
            printf("Server closed connection\n");
            fclose(f);
            return;
        }
        
        if (recv_pdu.msg_type != DUFTP_MSG_ACK) {
            printf("Unexpected response from server: %d\n", recv_pdu.msg_type);
            fclose(f);
            return;
        }
    }
    
    memset(&send_pdu, 0, sizeof(send_pdu));
    send_pdu.msg_type = DUFTP_MSG_COMPLETE;
    send_pdu.protocol_ver = DUFTP_PROTOCOL_VER;
    send_pdu.seq_num = sequence_number++;
    send_pdu.data_size = 0;
    
    dpsend(dpc, &send_pdu, sizeof(duftp_pdu));
    
    rcvSz = dprecv(dpc, &recv_pdu, sizeof(duftp_pdu));
    if (rcvSz == DP_CONNECTION_CLOSED) {
        printf("Server closed connection\n");
        fclose(f);
        return;
    }
    
    if (recv_pdu.msg_type != DUFTP_MSG_ACK) {
        printf("Unexpected response from server: %d\n", recv_pdu.msg_type);
        fclose(f);
        return;
    }
    
    printf("File transfer complete: %d bytes sent\n", total_bytes_sent);
    
    fclose(f);
    
    printf("Disconnecting from server...\n");
    dpdisconnect(dpc);
}

void start_server(dp_connp dpc){
    printf("Server started. Waiting for connections...\n");
    server_loop(dpc, sbuffer, rbuffer, sizeof(sbuffer), sizeof(rbuffer));
}


int main(int argc, char *argv[])
{
    prog_config cfg;
    int cmd;
    dp_connp dpc;
    int rc;

    // Process the parameters and init the header
    cmd = initParams(argc, argv, &cfg);

    printf("MODE %d\n", cfg.prog_mode);
    printf("PORT %d\n", cfg.port_number);
    printf("FILE NAME: %s\n", cfg.file_name);

    switch(cmd){
        case PROG_MD_CLI:
            // For client, we still need the file path to read from
            snprintf(full_file_path, sizeof(full_file_path), "./outfile/%s", cfg.file_name);
            dpc = dpClientInit(cfg.svr_ip_addr, cfg.port_number);
            rc = dpconnect(dpc);
            if (rc < 0) {
                perror("Error establishing connection");
                exit(-1);
            }

            start_client(dpc);
            exit(0);
            break;

        case PROG_MD_SVR:
            dpc = dpServerInit(cfg.port_number);
            rc = dplisten(dpc);
            if (rc < 0) {
                perror("Error establishing connection");
                exit(-1);
            }

            start_server(dpc);
            break;
        default:
            printf("ERROR: Unknown Program Mode. Mode set is %d\n", cmd);
            break;
    }
}