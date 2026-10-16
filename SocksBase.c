#include "SocksBase.h"
#include "libsock/Socks.h"

int socks_build_target_socket(int sock){
    int cli_sock = Build_Target_Socket(sock);
    if(cli_sock == BUILD_TARGET_SOCKET_ERROR){
        return SOCKS_BUILD_TARGET_SOCKET_ERROR;
    }
    return cli_sock;
}

/* one reverse SOCKS session: get a reverse socket from the cmd channel,
 * speak SOCKS5 on it, then bridge to the real target. */
MIC_THREAD_FUN_DEF(socks_check_and_tunnel,parm){
    struct rcsocktul *tunn = (struct rcsocktul *)parm;
    int new_sock, target_sock;

    if(tunn == NULL){
        MIC_THREAD_END();
        return 0;
    }

    new_sock = proto_get_rcsocket(tunn->URL, tunn->port, tunn->poolnum);
    if(new_sock == PROTO_GET_RCSOCKET_ERROR){
        free(tunn);
        MIC_THREAD_END();
        return 0;
    }

    target_sock = socks_build_target_socket(new_sock);
    if(target_sock != SOCKS_BUILD_TARGET_SOCKET_ERROR){
        tunn_sock_to_sock(target_sock, new_sock, API_get_usec_time());
    }
    else{
        API_socket_close(new_sock);
    }
    free(tunn);
    MIC_THREAD_END();
    return 0;
}

/* spawn a worker for one reverse session, `sock` is a heap struct rcsocktul */
int socks_build_rcsocks_tunnel(void *sock){
    MIC_THREAD_HANDLE_ID thread_id;
    if(MIC_THREAD_CREATE(thread_id, socks_check_and_tunnel, sock)){
        puts("创建隧道线程失败");
        return 0;
    }
    return 1;
}
