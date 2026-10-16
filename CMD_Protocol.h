#ifndef _CMD_PROTO_H_
#define _CMD_PROTO_H_

#include "generic.h"

#define MAX_CMD_PRO_LEN                 6

#define PROTO_INIT_CMD_SER_ERROR        0
#define PROTO_INIT_CMD_SER_OK           1
#define PROTO_INIT_CMD_RCSOCKET_ERROR  -1
#define PROTO_GET_RCSOCKET_ERROR       -1
#define PROTO_UNDERSTAND_ERROR         -2
#define PROTO_UNDERSTAND_BUT_DO_ERROR  -1
#define proto_UNDERSTAND_AND_DO_OK      1
#define PRO_SEND_RCCMD_ERROR           -1
#define PRO_SEND_RCCMD_OK               1

/* 6 byte cmd frame: TYPE(1) NAME(1) DATA(4) */
#define PRO_MAKE_NET                1

#define READ_ERROR_PLEASE_SAY_AGAIN 0   /* server -> client */
#define I_AM_NEW_RC_CMD_SOCK_CLIENT 1   /* client -> server */
#define CONFIRM_YOU_ARE_SOCK_CLIENT 2   /* server -> client */
#define START_A_NEW_SOCK_TUNNEL_NOW 3   /* server -> client */
#define I_AM_NEW_RC_SOCK_FOR_TUNNEL 4   /* client -> server, carries POOLNUM */
#define CONFIRM_YOU_ARE_SOCK_TUNNEL 5   /* server -> client */

int proto_init_cmd_server_for_rc(int port);
int proto_init_cmd_rcsocket(const char *URL,int port);
int proto_get_rcsocket(const char *URL,int port,int poolnum);
int proto_understand_and_do_it(int sock);
int proto_send_rccmd_poolnum(int poolnum);
void proto_init_mutex();
void proto_reset_cmd_socket();

#endif
