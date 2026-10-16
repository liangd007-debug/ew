#include "ssocksd_pro.h"

/* one local SOCKS5 session */
MIC_THREAD_FUN_DEF(check_and_tunnel,parm){
    int sock = *(int *)parm;
    int client_sock;

    free(parm);     /* socket number was handed over on heap */
    MIC_USLEEP(1);

    client_sock = socks_build_target_socket(sock);
    if(client_sock != SOCKS_BUILD_TARGET_SOCKET_ERROR){
        tunn_sock_to_sock(client_sock, sock, API_get_usec_time());
    }
    else{
        API_socket_close(sock);
    }
    MIC_THREAD_END();
    return 0;
}

/* forward SOCKS5 server: -s ssocksd -l 1080 */
int create_ssocksd_server(int port,int usec){
    int socket_desc, client_sock, c;
    struct sockaddr_in client;
    MIC_THREAD_HANDLE_ID thread_id;

    API_set_usec_time(usec);
    socket_desc = API_socket_init_server(port, 300);
    if(socket_desc == SOCKET_SERVER_INIT_ERROR){
        return CREATE_SSOCKSD_SERVER_ERROR;
    }
    c = sizeof(struct sockaddr_in);
    printf("正向 SOCKS5  0.0.0.0:%d <--[%4d 毫秒]-->\n",
           port, API_get_usec_time());

    while(1){
        client_sock = accept(socket_desc, (struct sockaddr *)&client,
                             (socklen_t *)&c);
        if(client_sock < 0){
            puts("连接接入失败, 稍候重试");
            MIC_USLEEP(100);
            continue;
        }
        /* hand the socket number to the worker on the heap,
         * the old &stack_variable way raced with the next accept() */
        {
            int *sockbuf = (int *)malloc(sizeof(int));
            if(sockbuf == NULL){
                API_socket_close(client_sock);
                continue;
            }
            *sockbuf = client_sock;
            if(MIC_THREAD_CREATE(thread_id, check_and_tunnel, sockbuf)){
                API_socket_close(client_sock);
                free(sockbuf);
                puts("创建工作线程失败");
                continue;
            }
        }
        MIC_USLEEP(1);
    }
    return CREATE_SSOCKSD_SERVER_OK;
}
