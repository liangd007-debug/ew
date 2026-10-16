#ifndef _SOCKSBASE_H_
#define _SOCKSBASE_H_

#include "generic.h"

#define SOCKS_BUILD_TARGET_SOCKET_ERROR   -1

struct rcsocktul{
    char URL[300];
    int port;
    int poolnum;    /* pool id carried from the cmd channel, must be int */
};

int socks_build_target_socket(int sock);
int socks_build_rcsocks_tunnel(void *sock);

#endif
