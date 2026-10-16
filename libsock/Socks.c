#include "Socks.h"

#define IPV4_TYPE  0x01
#define  URL_TYPE  0x03
#define IPV6_TYPE  0x04

#define TCP_TYPE   0x01

#define CONNECT_TARGET_OK        0x00  /* succeeded */
#define CONNECT_TARGET_ERROR     0x01  /* general failure */
#define NOT_SUPPORT_TUNNEL       0x02  /* ruleset deny */
#define NET_NOT_REACHABLE        0x03  /* network unreachable */
#define THE_HOST_NOT_REACHABLE   0x04  /* host unreachable */
#define REFUSE_BY_REMOTE         0x05  /* connection refused */
#define TTL_OVERTIME             0x06  /* timeout */
#define NOT_SUPPORT_CMD          0x07  /* cmd not supported */
#define NOT_SUPPORT_ATYP         0x08  /* atyp not supported */
#define UNASSIGNED_TYPE          0xff

#define HANDSHAKE_TIMEOUT_MS   10000

int Check_Version(int sock){
    char ERreq[2] = {0x05, 0xff};
    char OKreq[2] = {0x05, 0x00};
    char head[2];
    char methods[256];
    int nmethods;

    if(sock < 0) return CHECK_VERSION_ERROR;

    /* RFC1928: VER(1) NMETHODS(1) METHODS(n) -- read them step by step,
     * one big recv() would swallow the following request bytes. */
    if(API_socket_recv_full(sock, head, 2, HANDSHAKE_TIMEOUT_MS) != 2){
        return CHECK_VERSION_ERROR;
    }
    if(head[0] != SOCKS5_V){
        API_socket_send(sock, ERreq, 2);
        printf("暂不支持 SOCKSv4\n");
        return CHECK_VERSION_ERROR;
    }
    nmethods = (unsigned char)head[1];
    if(nmethods <= 0){
        API_socket_send(sock, ERreq, 2);
        return CHECK_VERSION_ERROR;
    }
    if(API_socket_recv_full(sock, methods, nmethods, HANDSHAKE_TIMEOUT_MS) != nmethods){
        return CHECK_VERSION_ERROR;
    }
    if(API_socket_send(sock, OKreq, 2) == 2){
        return CHECK_VERSION_OK;
    }
    return CHECK_VERSION_ERROR;
}

int Say_Cannot_Build_Target_Now(int sock,int type){
    char ERreq[4] = {0x05, 0x00, 0x00, 0x01};
    ERreq[1] = (char)type;
    return API_socket_send(sock, ERreq, 4);
}

int Say_Cannot_Build_Target_Reason(int sock){
#ifdef WIN32
    switch(WSAGetLastError()){
        case WSAENETUNREACH:  Say_Cannot_Build_Target_Now(sock, NET_NOT_REACHABLE);      break;
        case WSAEHOSTUNREACH: Say_Cannot_Build_Target_Now(sock, THE_HOST_NOT_REACHABLE); break;
        case WSAECONNREFUSED: Say_Cannot_Build_Target_Now(sock, REFUSE_BY_REMOTE);       break;
        case WSAETIMEDOUT:    Say_Cannot_Build_Target_Now(sock, TTL_OVERTIME);           break;
        default:              Say_Cannot_Build_Target_Now(sock, CONNECT_TARGET_ERROR);   break;
    }
#else
    switch(errno){
        case ENETUNREACH:  Say_Cannot_Build_Target_Now(sock, NET_NOT_REACHABLE);      break;
        case EHOSTUNREACH: Say_Cannot_Build_Target_Now(sock, THE_HOST_NOT_REACHABLE); break;
        case ECONNREFUSED: Say_Cannot_Build_Target_Now(sock, REFUSE_BY_REMOTE);       break;
        case ETIMEDOUT:    Say_Cannot_Build_Target_Now(sock, TTL_OVERTIME);           break;
        default:           Say_Cannot_Build_Target_Now(sock, CONNECT_TARGET_ERROR);   break;
    }
#endif
    return 1;
}

int Say_Build_Target_OK(int sock){
    char OKreq[10] = {0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    return API_socket_send(sock, OKreq, 10);
}

int Get_Target_Socket(int sock){
    char head[4];
    char addrURL[300];
    unsigned char portbuf[2];
    int des_port;
    unsigned int addrvalue;
    struct in_addr buf_addr;
    int url_len;
    int mode, cli_sock;
    /* 2. Request : VER CMD RSV ATYP -- exact read, TCP may split it */
    if(API_socket_recv_full(sock, head, 4, HANDSHAKE_TIMEOUT_MS) != 4){
        return GET_TARGET_SOCKET_ERROR;
    }
    mode = (unsigned char)head[1];

    /* 2.1 address */
    if(head[3] == IPV4_TYPE){
        if(API_socket_recv_full(sock, (char *)&addrvalue, 4, HANDSHAKE_TIMEOUT_MS) != 4){
            return GET_TARGET_SOCKET_ERROR;
        }
        buf_addr.s_addr = addrvalue;
        strncpy(addrURL, inet_ntoa(buf_addr), sizeof(addrURL) - 1);
        addrURL[sizeof(addrURL) - 1] = '\0';
    }
    else if(head[3] == URL_TYPE){
        if(API_socket_recv_full(sock, addrURL, 1, HANDSHAKE_TIMEOUT_MS) != 1){
            return GET_TARGET_SOCKET_ERROR;
        }
        url_len = (unsigned char)addrURL[0];
        if(url_len <= 0 || url_len > 255){
            Say_Cannot_Build_Target_Now(sock, NOT_SUPPORT_ATYP);
            return GET_TARGET_SOCKET_ERROR;
        }
        if(API_socket_recv_full(sock, addrURL, url_len, HANDSHAKE_TIMEOUT_MS) != url_len){
            return GET_TARGET_SOCKET_ERROR;
        }
        addrURL[url_len] = '\0';
    }
    else if(head[3] == IPV6_TYPE){
        Say_Cannot_Build_Target_Now(sock, NOT_SUPPORT_ATYP);
        printf("暂不支持 IPv6\n");
        return GET_TARGET_SOCKET_ERROR;
    }
    else{
        Say_Cannot_Build_Target_Now(sock, NOT_SUPPORT_ATYP);
        printf("不支持的地址类型 %02x\n", (unsigned char)head[3]);
        return GET_TARGET_SOCKET_ERROR;
    }

    /* 2.2 port */
    if(API_socket_recv_full(sock, (char *)portbuf, 2, HANDSHAKE_TIMEOUT_MS) != 2){
        Say_Cannot_Build_Target_Now(sock, UNASSIGNED_TYPE);
        return GET_TARGET_SOCKET_ERROR;
    }
    des_port = (int)portbuf[0] * 256 + (int)portbuf[1];

    /* 2.3 connect to the target */
    if(mode == TCP_TYPE){
        printf("  连接目标 ---> %s:%d\n", addrURL, des_port);
        cli_sock = API_socket_connect(addrURL, des_port);
    }
    else{
        Say_Cannot_Build_Target_Now(sock, NOT_SUPPORT_CMD);
        printf("仅支持 CONNECT(TCP) 请求\n");
        return GET_TARGET_SOCKET_ERROR;
    }

    if(cli_sock == SOCKET_CONNECT_ERROR){
        Say_Cannot_Build_Target_Reason(sock);
        return GET_TARGET_SOCKET_ERROR;
    }
    Say_Build_Target_OK(sock);
    return cli_sock;
}

int Build_Target_Socket(int sock){
    int cli_sock;
    if(Check_Version(sock) == CHECK_VERSION_OK){
        cli_sock = Get_Target_Socket(sock);
        if(cli_sock != GET_TARGET_SOCKET_ERROR){
            return cli_sock;
        }
    }
    return BUILD_TARGET_SOCKET_ERROR;
}
