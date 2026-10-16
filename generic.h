#ifndef _GENERIC_H_
#define _GENERIC_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#ifdef WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <unistd.h>
#else
    #include <pthread.h>
    #include <netdb.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <arpa/inet.h>
    #include <signal.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

/* my headers start from here */
#include "BaseAPI.h"
#include "CMD_Protocol.h"
#include "SocksBase.h"
#include "Sock_Tunnel.h"
#include "ssocksd_pro.h"
#include "Lcx_Base.h"
#include "rssocks_pro.h"
/* end my headers */

/* ---------- thread ---------- */
/* MIC_THREAD_CREATE returns TRUE(non-zero) when it FAILS */
#ifdef WIN32
    #define MIC_THREAD_FUN_DEF(fun_name,parm) \
        WINAPI DWORD fun_name(LPVOID parm)
    #define MIC_THREAD_HANDLE_ID HANDLE
    #define MIC_THREAD_CREATE(handle,fun_name,parm) \
        ((handle = CreateThread(NULL,0,fun_name,(void*)parm,0,NULL)) == NULL)
    #define MIC_THREAD_JOIN(handle) do{ if(handle) CloseHandle(handle); }while(0)
    #define MIC_THREAD_END() do{}while(0)
    #define MIC_SLEEP(x)    Sleep((x)*1000)
    #define MIC_USLEEP(x)   Sleep(x)
#else
    #define MIC_THREAD_FUN_DEF(fun_name,parm) \
        void *fun_name(void *parm)
    #define MIC_THREAD_HANDLE_ID pthread_t
    #define MIC_THREAD_CREATE(handle,fun_name,parm) \
        (pthread_create(&handle,NULL,fun_name,(void*)parm) != 0)
    #define MIC_THREAD_JOIN(handle) pthread_join(handle,NULL)
    #define MIC_THREAD_END() pthread_detach(pthread_self())
    #define MIC_SLEEP(x)    sleep(x)
    #define MIC_USLEEP(x)   usleep((x)*1000)
#endif

/* ---------- mutex ---------- */
#ifdef WIN32
    #define MIC_MUTEX_T             CRITICAL_SECTION
    #define MIC_MUTEX_INIT(p)       InitializeCriticalSection(p)
    #define MIC_MUTEX_LOCK(p)       EnterCriticalSection(p)
    #define MIC_MUTEX_UNLOCK(p)     LeaveCriticalSection(p)
#else
    #define MIC_MUTEX_T             pthread_mutex_t
    #define MIC_MUTEX_INIT(p)       pthread_mutex_init(p,NULL)
    #define MIC_MUTEX_LOCK(p)       pthread_mutex_lock(p)
    #define MIC_MUTEX_UNLOCK(p)     pthread_mutex_unlock(p)
#endif

/* ---------- global tunable ---------- */
#define MAX_POOL        1000
#define True              1
#define False             0
#define TUNMAX_BUF      8192

#define EW_VERSION_STR  "free2.0"

#endif /* _GENERIC_H_ */
