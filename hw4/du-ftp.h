#pragma once

#define PROG_MD_CLI     0
#define PROG_MD_SVR     1
#define DEF_PORT_NO     2080
#define FNAME_SZ        150
#define PROG_DEF_FNAME  "test.c"
#define PROG_DEF_SVR_ADDR   "127.0.0.1"

/* Protocol Message Types */
#define DUFTP_MSG_FILENAME  1
#define DUFTP_MSG_DATA      2
#define DUFTP_MSG_ERROR     3
#define DUFTP_MSG_COMPLETE  4
#define DUFTP_MSG_ACK       5
#define DUFTP_MSG_REQUEST   6

/* Error Codes */
#define DUFTP_ERR_NONE          0
#define DUFTP_ERR_FILE_NOT_FOUND 1
#define DUFTP_ERR_PERMISSION    2
#define DUFTP_ERR_DISK_FULL     3
#define DUFTP_ERR_UNKNOWN       4

/* Protocol Constants */
#define DUFTP_MAX_DATA_SIZE  4096
#define DUFTP_PROTOCOL_VER   1

/* Protocol Data Unit (PDU) */
typedef struct duftp_pdu {
    int msg_type;
    int protocol_ver;
    int seq_num;
    int data_size;
    int error_code;
    int total_size;
    char filename[FNAME_SZ];
    char data[DUFTP_MAX_DATA_SIZE];
} duftp_pdu;

typedef struct prog_config{
    int     prog_mode;
    int     port_number;
    char    svr_ip_addr[16];
    char    file_name[128];
} prog_config;

