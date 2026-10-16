#include "Lcx_Base.h"

/* ---- used by lcx_slave: where the control channel is, and where to forward ---- */
static char from_URL[300], to_URL[300];
static int from_port, to_port;

/* ---- lcx_listen : local socks entry + reverse control channel ---- */

/* accept local connections, ask the agent for one reverse socket each */
MIC_THREAD_FUN_DEF(create_socks_port_server,p_port){
    int port = *(int *)p_port;
    int socks_server, client_sock, poolnum;
    struct sockaddr_in client;
    int len_sockaddr = sizeof(struct sockaddr_in);
    MIC_THREAD_HANDLE_ID thread_id;

    free(p_port);
    socks_server = API_socket_init_server(port, 300);
    if(socks_server == SOCKET_SERVER_INIT_ERROR){
        printf("监听端口 %d 失败\n", port);
        return 0;
    }
    (void)thread_id;

    while(1){
        client_sock = accept(socks_server, (struct sockaddr *)&client,
                             (socklen_t *)&len_sockaddr);
        if(client_sock < 0){
            puts("连接接入失败, 稍候重试");
            MIC_USLEEP(100);
            continue;
        }
        poolnum = tunn_set_first_pool_and_lock_it(client_sock,
                                                  API_get_usec_time());
        if(poolnum == TUNNEL_GET_POOL_ID_FALSE){
            /* pool is full, nobody will pick this connection up */
            API_socket_close(client_sock);
        }
        else if(proto_send_rccmd_poolnum(poolnum) != PRO_SEND_RCCMD_OK){
            /* no agent connected / cmd channel broken:
             * tunn_close() releases the slot and closes client_sock */
            tunn_close(poolnum);
        }
        MIC_USLEEP(1);
    }
    return 0;
}

/* wait for the reverse control channel */
MIC_THREAD_FUN_DEF(create_listen_port,p_port){
    int port = *(int *)p_port;
    free(p_port);
    proto_init_cmd_server_for_rc(port);
    return 0;
}

int lcx_listen(int from_p,int cmd_p,int usec){
    MIC_THREAD_HANDLE_ID thread_1, thread_2;
    int *p1, *p2;

    API_set_usec_time(usec);
    printf("反向隧道控制端  入口 0.0.0.0:%d <--[%4d 毫秒]--> 回连 0.0.0.0:%d\n",
           from_p, API_get_usec_time(), cmd_p);

    p1 = (int *)malloc(sizeof(int));
    p2 = (int *)malloc(sizeof(int));
    if(p1 == NULL || p2 == NULL){
        puts("内存不足");
        return -1;
    }
    *p1 = from_p;
    *p2 = cmd_p;

    if(MIC_THREAD_CREATE(thread_1, create_socks_port_server, p1)){
        printf("监听端口 %d 失败\n", from_p);
        free(p1);
        return -1;
    }
    if(MIC_THREAD_CREATE(thread_2, create_listen_port, p2)){
        printf("监听端口 %d 失败\n", cmd_p);
        free(p2);
        return -1;
    }
    /* both workers live on their own, just sleep here */
    MIC_THREAD_JOIN(thread_1);
    MIC_THREAD_JOIN(thread_2);
    while(1){ MIC_SLEEP(1000); }
    return 1;
}

/* ---- lcx_tran : plain port forward, listen here, forward there ---- */

int lcx_tran(int from_p,const char *to_host,int to_p,int usec){
    int server, client_sock, target_sock;
    struct sockaddr_in client;
    int len_sockaddr = sizeof(struct sockaddr_in);

    API_set_usec_time(usec);
    printf("端口转发  0.0.0.0:%d <--[%4d 毫秒]--> %s:%d\n",
           from_p, API_get_usec_time(), to_host, to_p);

    server = API_socket_init_server(from_p, 300);
    if(server == SOCKET_SERVER_INIT_ERROR){
        return -1;
    }
    while(1){
        client_sock = accept(server, (struct sockaddr *)&client,
                             (socklen_t *)&len_sockaddr);
        if(client_sock < 0){
            puts("连接接入失败, 稍候重试");
            MIC_USLEEP(100);
            continue;
        }
        target_sock = API_socket_connect(to_host, to_p);
        if(target_sock != SOCKET_CONNECT_ERROR){
            tunn_sock_to_sock(target_sock, client_sock, API_get_usec_time());
        }
        else{
            API_socket_close(client_sock);
        }
        MIC_USLEEP(1);
    }
    return 1;
}

/* ---- lcx_slave : behind two hops, dial the control channel and the target ---- */

/* one reverse connection + one direct connection, glued together */
MIC_THREAD_FUN_DEF(check_and_slave_tunnel,parm){
    int poolnum = *(int *)parm;
    int target_sock, new_sock;

    free(parm);
    if(poolnum < 0){
        MIC_THREAD_END();
        return 0;
    }

    new_sock = proto_get_rcsocket(from_URL, from_port, poolnum);
    target_sock = API_socket_connect(to_URL, to_port);

    if(new_sock != SOCKET_CONNECT_ERROR &&
       target_sock != SOCKET_CONNECT_ERROR){
        tunn_sock_to_sock(new_sock, target_sock, API_get_usec_time());
    }
    else{
        API_socket_close(new_sock);
        API_socket_close(target_sock);
    }
    MIC_THREAD_END();
    return 0;
}

int lcx_slave(const char *mfrom_URL,int mfrom_port,
              const char *mto_URL,int mto_port,int usec){
    char cmd_buff[MAX_CMD_PRO_LEN];
    int readsize, poolnum, cmd_sock;

    API_set_usec_time(usec);
    memset(from_URL, 0, sizeof(from_URL));
    memset(to_URL, 0, sizeof(to_URL));
    strncpy(from_URL, mfrom_URL, sizeof(from_URL) - 1);
    strncpy(to_URL,   mto_URL,   sizeof(to_URL)   - 1);
    from_port = mfrom_port;
    to_port   = mto_port;
    printf("反向转发工作端  控制端 %s:%d <--[%4d 毫秒]--> 目标 %s:%d\n",
           from_URL, from_port, usec, to_URL, to_port);

    while(1){
        /* (re)connect the control channel, loop instead of recursive call */
        cmd_sock = proto_init_cmd_rcsocket(from_URL, from_port);
        if(cmd_sock == PROTO_INIT_CMD_RCSOCKET_ERROR){
            printf("控制通道不可达, 3 秒后重试: %s:%d\n", from_URL, from_port);
            MIC_SLEEP(3);
            continue;
        }

        while(1){
            readsize = API_socket_recv_full(cmd_sock, cmd_buff,
                                            MAX_CMD_PRO_LEN, 0);
            if(readsize != MAX_CMD_PRO_LEN){
                printf("控制通道已断开, 正在重连 ...\n");
                break;
            }
            if(cmd_buff[0] == PRO_MAKE_NET &&
               cmd_buff[1] == START_A_NEW_SOCK_TUNNEL_NOW){
                int *ppoolnum;
                MIC_THREAD_HANDLE_ID thread_id;

                poolnum = API_m_chartoi(&(cmd_buff[2]), 4);
                if(poolnum == CHAR_TO_I_ERROR) continue;

                ppoolnum = (int *)malloc(sizeof(int));
                if(ppoolnum == NULL) continue;
                *ppoolnum = poolnum;

                if(MIC_THREAD_CREATE(thread_id, check_and_slave_tunnel, ppoolnum)){
                    puts("创建隧道线程失败");
                    free(ppoolnum);
                }
                MIC_USLEEP(1);
            }
            else{
                printf("收到未知指令帧, 重建控制通道\n");
                break;
            }
        }
        API_socket_close(cmd_sock);
        cmd_sock = -1;
        MIC_SLEEP(1);
    }
    return 1;
}
