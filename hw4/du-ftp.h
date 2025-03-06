#pragma once

#define PROG_MD_CLI     0
#define PROG_MD_SVR     1
#define DEF_PORT_NO     2080
#define FNAME_SZ        150
#define PROG_DEF_FNAME  "test.c"
#define PROG_DEF_SVR_ADDR   "127.0.0.1"

#define DUFTP_MSG_FILENAME  1
#define DUFTP_MSG_DATA      2
#define DUFTP_MSG_ERROR     3
#define DUFTP_MSG_COMPLETE  4

#define DUFTP_ERR_NONE          0
#define DUFTP_ERR_FILE_NOT_FOUND 1
#define DUFTP_ERR_UNKNOWN       2

typedef struct duftp_pdu {
    int msg_type;
    int data_size;
    int error_code;
    char filename[FNAME_SZ];
} duftp_pdu;

typedef struct prog_config{
    int     prog_mode;
    int     port_number;
    char    svr_ip_addr[16];
    char    file_name[128];
} prog_config;

