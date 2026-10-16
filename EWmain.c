#include "generic.h"

static const char *optstring = "s:l:ahvt:d:e:f:g:";
static struct option opts[] = {
    {"state",        required_argument, NULL, 's'},
    {"listenport",   required_argument, NULL, 'l'},
    {"about",        no_argument,       NULL, 'a'},
    {"help",         no_argument,       NULL, 'h'},
    {"version",      no_argument,       NULL, 'v'},
    {"usectime",     required_argument, NULL, 't'},
    {"refhost",      required_argument, NULL, 'd'},
    {"refport",      required_argument, NULL, 'e'},
    {"connhost",     required_argument, NULL, 'f'},
    {"connport",     required_argument, NULL, 'g'},
    {0, 0, 0, 0}
};

#define NULL_STATE 0
#define SSOCKSD    1
#define RCSOCKS    2
#define RSSOCKS    3
#define LCX_LISTEN 4
#define LCX_TRAN   5
#define LCX_SLAVE  6

#define NO_NEEDHELP 0
#define NEEDHELP    (1 << 1)
#define ABOUT_NOW   (1 << 2)
#define VERSION     (1 << 3)

#define HAS_STATE(m,n) ((m) & (n))

static int cmdstate = NULL_STATE;
static int state = NO_NEEDHELP;
static char refHost[300]  = "127.0.0.1";
static int  refPort  = 1080;
static char connHost[300] = "127.0.0.1";
static int  connPort = 8888;
static int  listenPort = 8888;
static int  usec = 10000;    /* idle timeout of a tunnel, in milliseconds */

static int getStatenum(char *opt){
    if(!strcmp("ssocksd",    opt)) return SSOCKSD;
    if(!strcmp("rcsocks",    opt)) return RCSOCKS;
    if(!strcmp("rssocks",    opt)) return RSSOCKS;
    if(!strcmp("lcx_listen", opt)) return LCX_LISTEN;
    if(!strcmp("lcx_tran",   opt)) return LCX_TRAN;
    if(!strcmp("lcx_slave",  opt)) return LCX_SLAVE;
    return NULL_STATE;
}

static int help(){
    if(cmdstate == NULL_STATE){
        printf("\nEWhere %s - 内网穿透 / 端口转发工具\n", EW_VERSION_STR);
        printf("用法: EWhere -s <工作模式> [选项]\n\n");
        printf("工作模式 (-s):\n");
        printf("  ssocksd    正向 SOCKS5 代理\n");
        printf("             EWhere -s ssocksd -l 1080\n");
        printf("  rcsocks    反向 SOCKS5 代理 [控制端, 放公网]\n");
        printf("             EWhere -s rcsocks -l 1080 -e 8888\n");
        printf("  rssocks    反向 SOCKS5 代理 [工作端, 放内网]\n");
        printf("             EWhere -s rssocks -d <控制端IP> -e 8888\n");
        printf("  lcx_listen 反向隧道       [控制端, 放公网]\n");
        printf("             EWhere -s lcx_listen -l 1080 -e 8888\n");
        printf("  lcx_tran   端口转发\n");
        printf("             EWhere -s lcx_tran -l 1080 -f <目标IP> -g 80\n");
        printf("  lcx_slave  反向隧道       [工作端, 放内网]\n");
        printf("             EWhere -s lcx_slave -d <控制端IP> -e 8888 -f <目标IP> -g 80\n");
        printf("\n选项:\n");
        printf("  -l <端口>  本机监听端口\n");
        printf("  -d <地址>  rssocks / lcx_slave 连接的控制端地址\n");
        printf("  -e <端口>  rssocks / lcx_slave 连接的控制端端口\n");
        printf("  -f <地址>  lcx_tran / lcx_slave 的转发目标地址\n");
        printf("  -g <端口>  lcx_tran / lcx_slave 的转发目标端口\n");
        printf("  -t <毫秒>  隧道空闲超时时间, 默认 %d\n", usec);
        printf("  -h         显示本帮助; 加 -s 可查看该模式的详细示例\n");
        printf("  -a         显示关于信息\n");
        printf("  -v         显示版本号\n");
        printf("\n");
        return 1;
    }
    printf("\n");
    switch(cmdstate){
    case SSOCKSD:
        printf("模式 ssocksd - 正向 SOCKS5 代理\n");
        printf("  在本机开一个 SOCKS5 端口, 流量从本机出网\n");
        printf("  EWhere -s ssocksd -l 1080\n");
        printf("  使用: 浏览器/Proxifier 代理设为 本机:1080 (SOCKS5)\n");
        break;
    case RCSOCKS:
        printf("模式 rcsocks - 反向 SOCKS5 代理 [控制端]\n");
        printf("  放在有公网 IP 的机器上, 等内网的 rssocks 回连\n");
        printf("  EWhere -s rcsocks -l 1080 -e 8888\n");
        printf("    -l 1080  : 对外提供 SOCKS5 的端口\n");
        printf("    -e 8888  : 监听内网回连的端口\n");
        printf("  内网机器对应执行: EWhere -s rssocks -d <本机IP> -e 8888\n");
        printf("  使用: 浏览器代理设为 公网机:1080 (SOCKS5)\n");
        break;
    case RSSOCKS:
        printf("模式 rssocks - 反向 SOCKS5 代理 [工作端]\n");
        printf("  放在内网机器上, 主动回连公网控制端, 帮公网侧出网\n");
        printf("  EWhere -s rssocks -d <控制端IP> -e 8888\n");
        printf("  公网机器对应执行: EWhere -s rcsocks -l 1080 -e 8888\n");
        printf("  使用: 浏览器代理设为 公网机:1080 (SOCKS5)\n");
        break;
    case LCX_LISTEN:
        printf("模式 lcx_listen - 反向隧道 [控制端]\n");
        printf("  放在有公网 IP 的机器上, 把内网服务拉到公网端口\n");
        printf("  EWhere -s lcx_listen -l 1080 -e 8888\n");
        printf("    -l 1080  : 对外提供服务的端口\n");
        printf("    -e 8888  : 监听内网回连的端口\n");
        printf("  内网机器对应执行: EWhere -s lcx_slave -d <本机IP> -e 8888 -f 127.0.0.1 -g 22\n");
        printf("  使用: 直接访问 公网机:1080 即等于访问 内网机:22\n");
        break;
    case LCX_SLAVE:
        printf("模式 lcx_slave - 反向隧道 [工作端]\n");
        printf("  放在内网机器上, 把自己的服务经公网控制端映射出去\n");
        printf("  EWhere -s lcx_slave -d <控制端IP> -e 8888 -f 127.0.0.1 -g 22\n");
        printf("    -d/-e : 公网控制端地址 (对应 lcx_listen 的 -e 端口)\n");
        printf("    -f/-g : 要映射出去的内网服务 (如 127.0.0.1:22)\n");
        printf("  公网机器对应执行: EWhere -s lcx_listen -l 1080 -e 8888\n");
        printf("  使用: 直接访问 公网机:1080 即等于访问 内网机:22\n");
        break;
    case LCX_TRAN:
        printf("模式 lcx_tran - 端口转发\n");
        printf("  在本机监听一个端口, 收到的连接全部转发到目标\n");
        printf("  EWhere -s lcx_tran -l 1080 -f <目标IP> -g 80\n");
        printf("  使用: 直接访问 本机:1080 即等于访问 目标IP:80\n");
        break;
    }
    printf("\n");
    return 1;
}

static int about_fun(){
    printf("\n");
    printf("EWhere %s - 内网穿透 / 端口转发工具\n", EW_VERSION_STR);
    printf("支持正向 / 反向 / 多级级联三种方式打通网络隧道,\n");
    printf("可在内网或外网快速建立 SOCKS5 代理服务。\n");
    printf("仅用于合法授权的测试与教学, 请遵守相关法律。\n");
    printf("详细用法请看 -h, 某个模式的用法请看 -h -s <模式>。\n");
    printf("\n");
    printf("致谢 (原 EarthWorm 项目):\n");
    printf("  rootkiter  : 原作者\n");
    printf("  asky       : 《Linux Programming by Example》\n");
    printf("  darksn0w   : 提供建议\n");
    printf("  zhuanjia   : 修改 Readme\n");
    printf("  syc4mor3   : 为工具命名\n");
    printf("  ...... \n");
    printf("官方主页: http://rootkiter.com/EarthWorm/\n");
    printf("\n");
    return 1;
}

static int realDo(){
    switch(cmdstate){
    case LCX_LISTEN:
        /* local socks entry + control channel, works with rssocks */
        lcx_listen(listenPort, refPort, usec);
        break;
    case LCX_SLAVE:
        lcx_slave(refHost, refPort, connHost, connPort, usec);
        break;
    case LCX_TRAN:
        lcx_tran(listenPort, connHost, connPort, usec);
        break;
    case SSOCKSD:
        create_ssocksd_server(listenPort, usec);
        break;
    case RCSOCKS:
        /* same as lcx_listen: accept socks entry, order the agent around */
        lcx_listen(listenPort, refPort, usec);
        break;
    case RSSOCKS:
        create_rssocks_server(refHost, refPort, usec);
        break;
    default:
        help();
        break;
    }
    return 1;
}

int main(int argc,char *argv[]){
    int c, index;

    API_env_init();
    tunn_init_Pool();
    proto_init_mutex();

    if(argc == 1){ help(); return 0; }

    while((c = getopt_long(argc, argv, optstring, opts, &index)) != -1){
        switch(c){
        case 'd':
            snprintf(refHost, sizeof(refHost), "%s", optarg);
            break;
        case 'e':
            refPort = atoi(optarg);
            break;
        case 'f':
            snprintf(connHost, sizeof(connHost), "%s", optarg);
            break;
        case 'g':
            connPort = atoi(optarg);
            break;
        case 's':
            cmdstate = getStatenum(optarg);
            if(cmdstate == NULL_STATE){
                printf("未知的工作模式: %s\n", optarg);
                state |= NEEDHELP;
            }
            break;
        case 'l':
            listenPort = atoi(optarg);
            break;
        case 't':
            usec = atoi(optarg);
            if(usec <= 0) usec = 10000;
            break;
        case 'h':
            state |= NEEDHELP;
            break;
        case 'a':
            state |= ABOUT_NOW;
            break;
        case 'v':
            state |= VERSION;
            break;
        case '?':
        default:
            state |= NEEDHELP;
            break;
        }
    }

    if(HAS_STATE(state, ABOUT_NOW)) about_fun();
    if(HAS_STATE(state, VERSION))   printf("\nEWhere %s\n\n", EW_VERSION_STR);
    if(HAS_STATE(state, NEEDHELP))      help();
    else if(cmdstate != NULL_STATE)     realDo();
    /* -v / -a alone : nothing more to do */

    return 0;
}
