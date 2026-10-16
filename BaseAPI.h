#ifndef _BASEAPI_H_
#define _BASEAPI_H_

#include "generic.h"

#define ENV_INIT_OK                1
#define ENV_INIT_FALSE             0
#define SOCKET_CONNECT_ERROR      -1
#define SOCKET_SERVER_INIT_ERROR  -1
#define SOCKET_SEND_ERROR         -1
#define SOCKET_RECV_ERROR         -1
#define SOCKET_INIT_ERROR         -1
#define SOCKET_OVER_TIME           0
#define SOCKET_MAY_CLOSE_NOW       1
#define SOCKET_CAN_READ_STATE      2
#define SOCKET_CAN_WRITE_STATE     2
#define I_TO_CHAR_ERROR           -1
#define I_TO_CHAR_OK               1
#define CHAR_TO_I_ERROR           -1

int API_env_init();

/* socket */
int API_socket_connect(const char *url,int port);
int API_socket_connect_to(int sock,const char *url,int port);
int API_socket_init_server(int port,int maxlisten);
int API_socket_send(int sock,const char *buf,int buflen);
int API_socket_recv(int sock,char *buf,int buflen);
int API_socket_recv_full(int sock,char *buf,int buflen,int timeout_ms);
int API_socket_close(int sock);
int API_socket_read_state(int sock,int sec,int usec);
int API_socket_write_state(int sock,int sec,int usec);

/* math : 4x7bit encode/decode for pool number */
int API_m_itochar(int a,char *b,int len);
int API_m_chartoi(char *b,int len);

/* idle-timeout (milliseconds) of a tunnel */
int API_set_usec_time(int msec);
int API_get_usec_time();

/* monotonic-ish millisecond clock, for idle timeout accounting */
long long API_now_ms();

#endif
