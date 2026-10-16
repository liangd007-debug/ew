#include "Sock_Tunnel.h"

sockbind_s socks_Pool[MAX_POOL];
static int live_num = 0;
static MIC_MUTEX_T pool_mutex;

int tunn_init_Pool(){
    int i;
    MIC_MUTEX_INIT(&pool_mutex);
    for(i = 0; i < MAX_POOL; i++){
        socks_Pool[i].sock_1    = -1;
        socks_Pool[i].sock_2    = -1;
        socks_Pool[i].flag      = False;
        socks_Pool[i].state     = NULL_SOCK;
        socks_Pool[i].usec_time = -1;
    }
    return TUNNEL_INIT_OK;
}

int tunn_close(int num){
    int flag = 0;

    if(num < 0 || num >= MAX_POOL) return TUNNEL_CLOSE_FALSE;

    MIC_MUTEX_LOCK(&pool_mutex);
    if(socks_Pool[num].sock_1 >= 0){
        API_socket_close(socks_Pool[num].sock_1);
        socks_Pool[num].sock_1 = -1;
        flag = 1;
    }
    if(socks_Pool[num].sock_2 >= 0){
        API_socket_close(socks_Pool[num].sock_2);
        socks_Pool[num].sock_2 = -1;
        flag = 1;
    }
    socks_Pool[num].usec_time = -1;
    socks_Pool[num].state     = NULL_SOCK;
    socks_Pool[num].flag      = False;
    if(flag && live_num > 0) live_num--;
    printf("--> %3d <-- (关闭) 已用/空闲  %d/%d\n",
        num, live_num, MAX_POOL - live_num);
    MIC_MUTEX_UNLOCK(&pool_mutex);
    return TUNNEL_CLOSE_OK;
}

/* bridge two sockets in both directions until idle timeout or close */
MIC_THREAD_FUN_DEF(tunn_run_now,p_num){
    int num = *(int *)p_num;
    int read_size, write_size;
    int n1, n2;
    int sock1, sock2, idle_ms;
    long long last_active;
    char buffer[TUNMAX_BUF];

    free(p_num);    /* pool id was handed over on heap */

    if(num < 0 || num >= MAX_POOL){
        MIC_THREAD_END();
        return 0;
    }
    sock1    = socks_Pool[num].sock_1;
    sock2    = socks_Pool[num].sock_2;
    idle_ms  = socks_Pool[num].usec_time;

    last_active = API_now_ms();
    while(1){
        n1 = API_socket_read_state(sock1, 0, 100);
        n2 = API_socket_read_state(sock2, 0, 100);
        if(n1 == SOCKET_INIT_ERROR || n2 == SOCKET_INIT_ERROR){
            break;
        }

        if(n1 == SOCKET_CAN_READ_STATE &&
          (read_size = API_socket_recv(sock1, buffer, TUNMAX_BUF)) > 0){
            last_active = API_now_ms();
            write_size = API_socket_send(sock2, buffer, read_size);
            if(write_size != read_size){
                break;
            }
        }
        else if(n1 == SOCKET_MAY_CLOSE_NOW ||
               (n1 == SOCKET_CAN_READ_STATE && read_size <= 0)){
            break;  /* the other side closed the session */
        }

        if(n2 == SOCKET_CAN_READ_STATE &&
          (read_size = API_socket_recv(sock2, buffer, TUNMAX_BUF)) > 0){
            last_active = API_now_ms();
            write_size = API_socket_send(sock1, buffer, read_size);
            if(write_size != read_size){
                break;
            }
        }
        else if(n2 == SOCKET_MAY_CLOSE_NOW ||
               (n2 == SOCKET_CAN_READ_STATE && read_size <= 0)){
            break;
        }

        /* real idle timeout in milliseconds */
        if(idle_ms > 0 && API_now_ms() - last_active >= idle_ms){
            break;
        }
        MIC_USLEEP(1);
    }

    MIC_THREAD_END();
    tunn_close(num);
    return 0;
}

/* grab a free pool slot and mark it busy, caller holds the write lock inside */
int tunn_get_pool_id_and_lock_it(){
    int i;
    while(1){
        MIC_MUTEX_LOCK(&pool_mutex);
        if(live_num < MAX_POOL - 1){
            for(i = 0; i < MAX_POOL; i++){
                if(socks_Pool[i].flag == False){
                    live_num++;
                    socks_Pool[i].flag = True;
                    printf("<-- %3d --> (建立) 已用/空闲  %d/%d\n",
                        i, live_num, MAX_POOL - live_num);
                    MIC_MUTEX_UNLOCK(&pool_mutex);
                    return i;
                }
            }
        }
        MIC_MUTEX_UNLOCK(&pool_mutex);
        MIC_USLEEP(10);     /* pool is full, wait for a slot */
    }
}

int tunn_set_first_pool_and_lock_it(int sock,int usec){
    int id = tunn_get_pool_id_and_lock_it();

    if(id != TUNNEL_GET_POOL_ID_FALSE){
        MIC_MUTEX_LOCK(&pool_mutex);
        socks_Pool[id].flag      = True;
        socks_Pool[id].sock_1    = sock;
        socks_Pool[id].state     = SOCK_1_OK;
        socks_Pool[id].usec_time = usec;
        MIC_MUTEX_UNLOCK(&pool_mutex);
    }
    return id;
}

/* pool_num comes from the network in reverse modes: validate before use */
int tunn_set_second_pool_and_run_it(int num,int sock){
    int *poolnum;
    MIC_THREAD_HANDLE_ID thread_1;

    if(num < 0 || num >= MAX_POOL){
        printf("非法的隧道编号 %d, 已丢弃该连接\n", num);
        API_socket_close(sock);
        return TUNNEL_SET_SECOND_POOL_AND_RUN_IT_FALSE;
    }

    MIC_MUTEX_LOCK(&pool_mutex);
    if(socks_Pool[num].state != SOCK_1_OK){
        MIC_MUTEX_UNLOCK(&pool_mutex);
        printf("隧道 %d 未在等待反向连接, 已丢弃\n", num);
        API_socket_close(sock);
        return TUNNEL_SET_SECOND_POOL_AND_RUN_IT_FALSE;
    }
    socks_Pool[num].sock_2 = sock;
    socks_Pool[num].state  = SOCK_2_OK;
    MIC_MUTEX_UNLOCK(&pool_mutex);

    poolnum = (int *)malloc(sizeof(int));
    if(poolnum == NULL){
        tunn_close(num);
        return TUNNEL_SET_SECOND_POOL_AND_RUN_IT_FALSE;
    }
    *poolnum = num;

    if(MIC_THREAD_CREATE(thread_1, tunn_run_now, poolnum)){
        perror("创建隧道线程失败");
        free(poolnum);
        tunn_close(num);
        return TUNNEL_SET_SECOND_POOL_AND_RUN_IT_FALSE;
    }
    return TUNNEL_SET_SECOND_POOL_AND_RUN_IT_OK;
}

int tunn_sock_to_sock(int from_sock,int to_sock,int usec){
    int num = tunn_set_first_pool_and_lock_it(from_sock, usec);
    if(num != TUNNEL_GET_FIRST_POOL_ID_FALSE){
        if(tunn_set_second_pool_and_run_it(num, to_sock) !=
                TUNNEL_SET_SECOND_POOL_AND_RUN_IT_FALSE){
            return TUNNEL_SOCK_TO_SOCK_OK;
        }
    }
    return TUNNEL_SOCK_TO_SOCK_ERROR;
}
