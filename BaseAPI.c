#include "BaseAPI.h"

/* idle timeout for one tunnel, in milliseconds */
static int idle_timeout_ms = 10000;

int API_env_init(){
    /* keep logs visible even when stdout is redirected to a file,
     * tunnel open/close events must show up at once (live demo use).
     * MinGW treats _IOLBF as fully buffered, so force _IONBF on Windows */
#ifdef WIN32
    setvbuf(stdout, NULL, _IONBF, 0);
#else
    setvbuf(stdout, NULL, _IOLBF, 0);
#endif
#ifdef WIN32
    WSADATA wsd;
    /* the program strings are compiled in GBK, keep the console on 936 too,
     * otherwise Chinese text turns to mojibake on Win7/GBK consoles */
    SetConsoleOutputCP(936);
    if (WSAStartup(MAKEWORD(2,2), &wsd) != 0){
        printf("初始化套接字库失败!\n");
        return ENV_INIT_FALSE;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    return ENV_INIT_OK;
}

/* resolve host name or ip string to IPv4 address */
static int resolve_host(const char *host, struct in_addr *out){
    struct addrinfo hints, *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(host, NULL, &hints, &result) != 0 || result == NULL){
        return SOCKET_CONNECT_ERROR;
    }
    *out = ((struct sockaddr_in *)(result->ai_addr))->sin_addr;
    freeaddrinfo(result);
    if(out->s_addr == 0){
        return SOCKET_CONNECT_ERROR;
    }
    return 1;
}

/* connect with a fixed timeout, so a dead target can not hang a worker */
static int connect_with_timeout(int sock,const struct sockaddr_in *server,int timeout_ms){
    struct timeval tv;
    fd_set wset;
    int err = 0;
    socklen_t errlen = sizeof(err);

#ifdef WIN32
    {
        unsigned long nb = 1;
        ioctlsocket(sock, FIONBIO, &nb);
    }
#else
    fcntl(sock, F_SETFL, O_NONBLOCK);
#endif

    if(connect(sock,(const struct sockaddr *)server,sizeof(*server)) < 0){
#ifdef WIN32
        if(WSAGetLastError() != WSAEWOULDBLOCK){
            return SOCKET_CONNECT_ERROR;
        }
#else
        if(errno != EINPROGRESS){
            return SOCKET_CONNECT_ERROR;
        }
#endif
        FD_ZERO(&wset);
        FD_SET(sock, &wset);
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if(select(sock + 1, NULL, &wset, NULL, &tv) <= 0){
            return SOCKET_CONNECT_ERROR;   /* timeout or select error */
        }
        if(getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen) < 0 || err != 0){
            return SOCKET_CONNECT_ERROR;
        }
    }

    /* back to blocking mode */
#ifdef WIN32
    {
        unsigned long nb = 0;
        ioctlsocket(sock, FIONBIO, &nb);
    }
#else
    fcntl(sock, F_SETFL, 0);
#endif
    return 1;
}

int API_socket_connect(const char *ser_addr,int port){
    struct sockaddr_in server_addr;
    int client_socket;

    client_socket = (int)socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket < 0){
        printf("创建 socket 失败!\n");
        return SOCKET_CONNECT_ERROR;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);
    if(resolve_host(ser_addr, &server_addr.sin_addr) == SOCKET_CONNECT_ERROR){
        printf("目标地址无效: %s\n", ser_addr);
        API_socket_close(client_socket);
        return SOCKET_CONNECT_ERROR;
    }

    if(connect_with_timeout(client_socket, &server_addr, 5000) == SOCKET_CONNECT_ERROR){
        printf("无法连接到 %s:%d\n", ser_addr, port);
        API_socket_close(client_socket);
        return SOCKET_CONNECT_ERROR;
    }
    return client_socket;
}

int API_socket_init_server(int port,int maxlisten){
    int socket_desc;
    struct sockaddr_in server;
    int on = 1;

    socket_desc = (int)socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc < 0){
        printf("创建 socket 失败 [端口 %d]\n", port);
        return SOCKET_SERVER_INIT_ERROR;
    }

    /* restart immediately after a restart, no TIME_WAIT headache */
    setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));

    memset(&server, 0, sizeof(server));
    server.sin_family      = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port        = htons(port);

    if(bind(socket_desc,(struct sockaddr *)&server, sizeof(server)) < 0){
        printf("绑定端口 %d 失败\n", port);
        API_socket_close(socket_desc);
        return SOCKET_SERVER_INIT_ERROR;
    }
    if(listen(socket_desc, maxlisten) < 0){
        printf("监听端口 %d 失败\n", port);
        API_socket_close(socket_desc);
        return SOCKET_SERVER_INIT_ERROR;
    }
    return socket_desc;
}

int API_socket_read_state(int sock,int sec,int usec){
    fd_set fds;
    struct timeval tv;
    int n;

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    tv.tv_sec  = sec;
    tv.tv_usec = usec;

    n = select(sock + 1, &fds, NULL, NULL, &tv);
    if(n > 0){
        if(FD_ISSET(sock, &fds)) return SOCKET_CAN_READ_STATE;
        return SOCKET_MAY_CLOSE_NOW;
    }
    else if(n == 0) return SOCKET_OVER_TIME;
    return SOCKET_INIT_ERROR;
}

int API_socket_write_state(int sock,int sec,int usec){
    fd_set fds;
    struct timeval tv;
    int n;

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    tv.tv_sec  = sec;
    tv.tv_usec = usec;

    n = select(sock + 1, NULL, &fds, NULL, &tv);
    if(n > 0){
        if(FD_ISSET(sock, &fds)) return SOCKET_CAN_WRITE_STATE;
        return SOCKET_MAY_CLOSE_NOW;
    }
    else if(n == 0) return SOCKET_OVER_TIME;
    return SOCKET_INIT_ERROR;
}

int API_socket_send(int sock,const char *buf,int buflen){
    return send(sock, buf, buflen, 0);
}

int API_socket_recv(int sock,char *buf,int buflen){
    return recv(sock, buf, buflen, 0);
}

/* read exactly buflen bytes, with an overall timeout in milliseconds.
 * timeout_ms <= 0 means "block until all bytes arrive".
 * returns buflen on success, -1 on error / peer closed, 0 on timeout. */
int API_socket_recv_full(int sock,char *buf,int buflen,int timeout_ms){
    int got = 0, n, remain, wait_ms, left_ms;

    if(buflen <= 0) return 0;

    /* blocking mode */
    if(timeout_ms <= 0){
        while(got < buflen){
            n = API_socket_recv(sock, buf + got, buflen - got);
            if(n <= 0) return SOCKET_RECV_ERROR;
            got += n;
        }
        return got;
    }

    /* timed mode : select first, then read */
    while(got < buflen){
        remain   = buflen - got;
        wait_ms  = (remain > 4096) ? 4096 : remain;   /* 4k per select step */
        left_ms  = timeout_ms;

        while(1){
            int step = (wait_ms < left_ms) ? wait_ms : left_ms;
            int st = API_socket_read_state(sock, 0, step * 1000);
            if(st == SOCKET_CAN_READ_STATE) break;
            if(st == SOCKET_INIT_ERROR)     return SOCKET_RECV_ERROR;
            left_ms -= step;
            if(left_ms <= 0) return SOCKET_OVER_TIME;
        }

        n = API_socket_recv(sock, buf + got, remain);
        if(n <= 0) return SOCKET_RECV_ERROR;
        got += n;
    }
    return got;
}

int API_socket_close(int sock){
    if(sock >= 0){
#ifdef WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
    return 1;
}

/* 4 * 7bit encode / decode, used to carry a pool number in the cmd protocol */
int API_m_itochar(int a,char *b,int len){
    int i;
    int buf = a;
    if(len != 4) return I_TO_CHAR_ERROR;
    if(a < 0)    return I_TO_CHAR_ERROR;
    for(i = 0; i < len; i++){
        b[i] = buf % 128;
        buf  = buf / 128;
    }
    return I_TO_CHAR_OK;
}

int API_m_chartoi(char *b,int len){
    int i;
    int result = 0;
    if(len != 4) return CHAR_TO_I_ERROR;
    for(i = len - 1; i >= 0; i--){
        if(b[i] < 0) return CHAR_TO_I_ERROR;
        result = result * 128 + b[i];
    }
    return result;
}

int API_set_usec_time(int msec){
    if(msec > 0) idle_timeout_ms = msec;
    return 1;
}

int API_get_usec_time(){
    return idle_timeout_ms;
}

long long API_now_ms(){
#ifdef WIN32
    return (long long)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
#endif
}
