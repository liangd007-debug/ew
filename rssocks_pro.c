#include "rssocks_pro.h"

/* reverse SOCKS agent: -s rssocks -d <control host> -e <control port>
 * keeps the control channel alive, opens one reverse socket per request
 * and speaks SOCKS5 on each of them. */
int create_rssocks_server(char *URL,int port,int usec){
    int cmd_sock;
    char cmd_buff[MAX_CMD_PRO_LEN];
    int readsize;

    API_set_usec_time(usec);

    while(1){
        /* (re)connect the control channel */
        cmd_sock = proto_init_cmd_rcsocket(URL, port);
        if(cmd_sock == PROTO_INIT_CMD_RCSOCKET_ERROR){
            printf("控制通道不可达, 3 秒后重试: %s:%d\n", URL, port);
            MIC_SLEEP(3);
            continue;
        }
        printf("反向 SOCKS5 工作端  控制端 %s:%d <--[%4d 毫秒]-->\n",
               URL, port, API_get_usec_time());

        while(1){
            readsize = API_socket_recv_full(cmd_sock, cmd_buff,
                                            MAX_CMD_PRO_LEN, 0);
            if(readsize != MAX_CMD_PRO_LEN){
                printf("控制通道已断开, 正在重连 ...\n");
                break;  /* fall through to reconnect */
            }
            if(cmd_buff[0] == PRO_MAKE_NET &&
               cmd_buff[1] == START_A_NEW_SOCK_TUNNEL_NOW){
                struct rcsocktul *tunn;
                int poolnum = API_m_chartoi(&(cmd_buff[2]), 4);

                if(poolnum == CHAR_TO_I_ERROR) continue;

                tunn = (struct rcsocktul *)malloc(sizeof(struct rcsocktul));
                if(tunn == NULL) continue;
                memset(tunn, 0, sizeof(struct rcsocktul));
                strncpy(tunn->URL, URL, sizeof(tunn->URL) - 1);
                tunn->port    = port;
                tunn->poolnum = poolnum;

                if(socks_build_rcsocks_tunnel(tunn) == 0){
                    puts("创建隧道线程失败");
                    free(tunn);
                }
                /* worker owns tunn from here on */
                MIC_USLEEP(1);
            }
            else{
                printf("收到未知指令帧, 重建控制通道\n");
                break;
            }
        }
        API_socket_close(cmd_sock);
        cmd_sock = -1;
        proto_reset_cmd_socket();
        MIC_SLEEP(1);
    }
    return 1;
}
