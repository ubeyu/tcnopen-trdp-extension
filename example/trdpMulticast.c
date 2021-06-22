/******************************************************************************/
/** 文件名   trdpMulticast.c                                                    **/
/** 作  者   王浩洋                                                          **/
/** 版  本   PD V1.0.0                                                          **/
/** 日  期   2021 1                                                 **/
/** 更  新   2021 5                                                 **/
/******************************************************************************/

/***********************************************************************************************************************
 * 引用库函数
 */
#include <stdio.h>
#include <string.h>

#include "trdp_if_light.h"

#if defined (POSIX)
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <time.h>
#endif

#include "vos_thread.h"
#include "trdpProc.h"
/* --- 全局变量 ---------------------------------------------------------------*/



//内存结构体配置
TRDP_MEM_CONFIG_T memcfg;

//会话/应用程序变量存储
TRDP_APP_SESSION_T apph;

//默认PD配置
TRDP_PD_CONFIG_T pdcfg;

//库初始化的各种标志/常规TRDP选项
TRDP_PROCESS_CONFIG_T proccfg;

//默认地址-可从命令行重写
TRDP_IP_ADDR_T srcip;
TRDP_IP_ADDR_T dstip;
TRDP_IP_ADDR_T mcast;

typedef enum
{
    PORT_PUSH,                      /* 输出端口 ('Pd'/push)（TSN支持）*/
    PORT_PULL,                      /* 输出端口 ('Pp'/pull)   */
    PORT_REQUEST,                   /* 输出端口 ('Pr'/request)*/
    PORT_SINK,                      /* 输入端口               */
    PORT_SINK_PUSH,                 /* 推送消息的输入端口（TSN支持）*/
} Type;

static const char * types[] =
    { "Pd ->", "Pp ->", "Pr ->", "   <-", "   <-" };

typedef struct
{
    Type type;                      /* 端口类型 */
    TRDP_ERR_T err;                 /* 放置/获取状态 */
    TRDP_PUB_T ph;                  /* 发布 handle */
    TRDP_SUB_T sh;                  /* 订阅 handle */
    UINT32 comid;                   /* 端口号            */
    UINT32 repid;                   /* 回复端口号 (for PULL requests) */
    UINT32 size;                    /* 大小                            */
    TRDP_IP_ADDR_T src;             /* 源 IP 地址            */
    TRDP_IP_ADDR_T dst;             /* 目标 IP 地址          */
    TRDP_IP_ADDR_T rep;             /* 回复 IP 地址 (for PULL requests) */
    UINT32 cycle;                   /* 周期                                */
    UINT32 timeout;                 /* 超时（对于接收器端口） (for SINK ports)             */
    unsigned char data[TRDP_MAX_PD_DATA_SIZE];       /* 数据缓冲器                       */
    int link;                       /* 链接端口的索引（echo或subscribe） (echo or subscribe) */
} Port;

int size[3] = { 0, 256, TRDP_MAX_PD_DATA_SIZE };     /* 小/中/大数据集 */
int period[2]  = { 100, 250 };      /* 快/慢循环周期          */
unsigned cycle = 0;

Port ports[64];                     /* 端口列表         */
int nports = 0;                     /* 端口数        */

#ifdef TSN_SUPPORT
    #define PORT_FLAGS TRDP_FLAGS_TSN
#else
    #define PORT_FLAGS TRDP_FLAGS_NONE
#endif

/***********************************************************************************************************************
 * 函数原型
 */
void gen_push_ports_master(UINT32 comid, UINT32 echoid);
void gen_pull_ports_slave(UINT32 reqid, UINT32 repid);
void gen_push_ports_slave(UINT32 comid, UINT32 echoid);

/* --- 生成推送端口 ---------------------------------------------------*/

void gen_push_ports_master(UINT32 comid, UINT32 echoid)
{
    Port src, snk;
    int num = nports;
    // UINT32  a, sz, per;
    
    printf("- 生成推端口 (master side) ... ");

    memset(&src, 0, sizeof(src));
    memset(&snk, 0, sizeof(snk));

    src.type = PORT_PUSH;
    snk.type = PORT_SINK_PUSH;
    snk.timeout = 4000000;         /* 4 秒延时 */

    /* 数据大小 */
    int pushSz = 2;
    /* 周期 period [usec] */
    int pushPer = 0;
    /* 端口号  */
    // src.comid = comid + 100u *a+40*(per+1)+3*(sz+1);
    // snk.comid = echoid + 100u *a+40*(per+1)+3*(sz+1);
    src.comid = comid;
    snk.comid = echoid;
    /* 数据大小 */
    src.size = snk.size = (UINT32) size[pushSz];
    /* 周期 period [usec] */
    src.cycle = (UINT32) 1000u * (UINT32)period[pushPer];
    /* 组播地址 multicast address */
    src.dst = snk.dst = mcast;
    src.src = srcip;
    snk.src = dstip;

    src.link = -1;
    /* 添加端口到配置中 add ports to config */
    ports[nports++] = src;
    ports[nports++] = snk;            

    printf("%u 主推送端口创建\n", nports - num);
}

void gen_push_ports_slave(UINT32 comid, UINT32 echoid)
{
    Port src, snk;
    int num = nports;
    // UINT32 a, sz, per;

    printf("- 生成推端口  (slave side) ... ");

    memset(&src, 0, sizeof(src));
    memset(&snk, 0, sizeof(snk));

    src.type = PORT_PUSH;
    snk.type = PORT_SINK_PUSH;
    snk.timeout = 4000000;         /* 4 秒延时 */

    /* 数据大小 */
    int pushSz = 2;
    /* 周期 period [usec] */
    int pushPer = 0;
    /* 端口号  */
    // src.comid = comid + 100u *a+40*(per+1)+3*(sz+1);
    // snk.comid = echoid + 100u *a+40*(per+1)+3*(sz+1);
    src.comid = comid;
    snk.comid = echoid;
    /* 数据大小 */
    src.size = snk.size = (UINT32) size[pushSz];
    /* 周期 period [usec] */
    src.cycle = (UINT32) 1000u * (UINT32)period[pushPer];
    /* 组播地址 multicast address */
    src.dst = snk.dst = mcast;
    src.src = srcip;
    snk.src = dstip;

    src.link = -1;
    /* 添加端口到配置中 add ports to config */
    ports[nports++] = snk;
    src.link = nports - 1;
    ports[nports++] = src;   

    printf("%u 从推送端口创建\n", nports - num);
}

/* --- 生成拉入端口 ---------------------------------------------------*/

static void gen_pull_ports_master(UINT32 reqid, UINT32 repid)
{
    Port req, rep;
    int num = nports;
    UINT32  a, sz;

    printf("- 生成拉端口 (master side) ... ");

    memset(&req, 0, sizeof(req));
    memset(&rep, 0, sizeof(rep));

    req.type = PORT_REQUEST;
    rep.type = PORT_SINK;

    /* 对于单播/组播地址 for unicast/multicast address */
    for (a = 0; a < 2; ++a)
    {   /* 对于所有数据集大小 for all dataset sizes */
        for (sz = 0; sz < 2; ++sz)
        {   /* 端口号 */
            req.comid = reqid + 100*a + 3*(sz+1);
            rep.comid = repid + 100*a + 3*(sz+1);
            /* 数据大小 */
            req.size = (UINT32) size[sz];
            rep.size = (UINT32) size[sz + 1];
            /* 地址 addresses */
            if (!a)
            {   /* 单播地址 unicast address */
                req.dst = dstip;
                req.src = srcip;
                req.rep = srcip;
                req.repid = rep.comid;
                rep.src = dstip;
                rep.dst = srcip;
            }
            else
            {   /* 组播地址 multicast address */
                req.dst = mcast;
                req.src = srcip;
                req.rep = mcast;
                req.repid = rep.comid;
                rep.dst = mcast;
                rep.src = dstip;
            }
            /* 添加端口到配置中 add ports to config */
            ports[nports++] = rep;
            req.link = nports - 1;
            ports[nports++] = req;
        }
    }

    printf("%u 主拉入端口创建 \n", nports - num);
}

void gen_pull_ports_slave(UINT32 reqid, UINT32 repid)
{
    Port req, rep;
    int num = nports;
    UINT32 a, sz;

    printf("- 生成拉端口 (slave side) ... ");

    memset(&req, 0, sizeof(req));
    memset(&rep, 0, sizeof(rep));

    req.type = PORT_SINK;
    rep.type = PORT_PULL;
    req.timeout = 4000000;      /* 4 秒延时 */

    /* 对于单播/组播地址 for unicast/multicast address */
    for (a = 0; a < 2; ++a)
    {   /* 对于所有数据集大小 for all dataset sizes */
        for (sz = 0; sz < 2; ++sz)
        {   /* 端口号 comid */
            req.comid = reqid + 100*a + 3*(sz+1);
            rep.comid = repid + 100*a + 3*(sz+1);
            /* 数据大小 dataset size */
            req.size = (UINT32) size[sz];
            rep.size = (UINT32) size[sz + 1];
            /* 地址 addresses */
            if (!a)
            {   /* 单播地址 unicast address */
                req.dst = srcip;
                req.src = dstip;
                rep.src = srcip;
                rep.dst = 0;
            }
            else
            {   /* 组播地址 multicast address */
                req.dst = mcast;
                req.src = dstip;
                rep.src = srcip;
                rep.dst = 0;
            }
            /* 添加端口到配置中 add ports to config */
            ports[nports++] = req;
            rep.link = nports - 1;
            ports[nports++] = rep;
        }
    }

    printf("%u 从拉入端口创建\n", nports - num);
}

/* --- 设置端口 -----------------------------------------------------------*/

static void setup_ports()
{
    int i;
    printf("- 设置端口:\n");
    /* 逐个设置端口 setup ports one-by-one */
    for (i = 0; i < nports; ++i)
    {
        Port * p = &ports[i];
        TRDP_COM_PARAM_T comPrams = TRDP_PD_DEFAULT_SEND_PARAM;
#if PORT_FLAGS == TRDP_FLAGS_TSN
        comPrams.vlan = 1;
        comPrams.tsn = TRUE;
#endif

        printf("  %3d: <%d> / %s / %4d / %3d ... ",
            i, p->comid, types[p->type], p->size, p->cycle / 1000);

        /* 取决于端口类型 depending on port type */
        switch (p->type)
        {
        case PORT_PUSH:
            p->err = tlp_publish(
                apph,               /* 会话句柄 session handle */
                &p->ph,             /* 发布句柄 publish handle */
                NULL, NULL,
                0u,                 /* 服务ID serviceId        */
                p->comid,           /* 端口号 comid            */
                0,                  /* 拓扑计数器 topo counter     */
                0,
                p->src,             /* 源地址 source address   */
                p->dst,             /* 目标地址 destination address */
                p->cycle,           /* 循环周期 cycle period   */
                0,                  /* 冗余 redundancy     */
                PORT_FLAGS,         /* 标志 flags          */
                &comPrams,          /* 默认发送参数 default send parameters */
                p->data,            /* 数据 data           */
                p->size);           /* 数据大小 data size      */

            if (p->err != TRDP_NO_ERR)
                printf("tlp_publish() failed, err: %d\n", p->err);
            else
                printf("ok\n");
            break;
        case PORT_PULL:
            p->err = tlp_publish(
                apph,               /* 会话句柄 session handle */
                &p->ph,             /* 发布句柄 publish handle */
                NULL, NULL, 
                0u,                 /* 服务ID serviceId        */
                p->comid,           /* 端口号 comid            */
                0,                  /* 拓扑计数器 topo counter     */
                0,
                p->src,             /* 源地址 source address   */
                p->dst,             /* 目标地址 destination address */
                p->cycle,           /* 循环周期 cycle period   */
                0,                  /* 冗余 redundancy     */
                TRDP_FLAGS_NONE,    /* 标志 flags          */
                NULL,               /* 默认发送参数 default send parameters */
                p->data,            /* 数据 data           */
                p->size);           /* 数据大小 data size      */

            if (p->err != TRDP_NO_ERR)
                printf("tlp_publish() failed, err: %d\n", p->err);
            else
                printf("ok\n");
            break;

        case PORT_REQUEST:
            p->err = tlp_request(
                apph,               /* 会话句柄 session handle */
                ports[p->link].sh,  /* 相关订阅句柄 related subscribe handle */
                0u,                 /* 服务ID serviceId        */
                p->comid,           /* 端口号 comid          */
                0,                  /* 拓扑计数器 topo counter   */
                0,
                p->src,             /* 源地址 source address */
                p->dst,             /* 目标地址 destination address */
                0,                  /* 冗余 redundancy     */
                TRDP_FLAGS_NONE,    /* 标志 flags          */
                NULL,               /* 默认发送参数 default send parameters */
                p->data,            /* 数据 data           */
                p->size,            /* 数据大小 data size      */
                p->repid,           /* 回复端口号 reply comid    */
                p->rep);            /* 回复ip地址 reply ip address  */

            if (p->err != TRDP_NO_ERR)
                printf("tlp_request() failed, err: %d\n", p->err);
            else
                printf("ok\n");
            break;

        case PORT_SINK:
            p->err = tlp_subscribe(
                apph,               /* 会话句柄 session handle   */
                &p->sh,             /* 订阅句柄 subscribe handle */
                NULL,               /* 用户引用 user ref         */
                NULL,               /* 回调函数 callback funktion */
                0u,                 /* 服务ID serviceId        */
                p->comid,           /* 端口号 comid            */
                0,                  /* 拓扑计数器 topo counter     */
                0,
                p->src,             /* 源地址 source address   */
                VOS_INADDR_ANY,
                p->dst,             /* 目标地址 destination address    */
                TRDP_FLAGS_NONE,    /* 未设置标志 No flags set     */
                NULL,               /* 接收参数 Receive params */
                p->timeout,             /* 超时[usec] timeout [usec]   */
                TRDP_TO_SET_TO_ZERO);   /* 超时行为 timeout behavior */

            if (p->err != TRDP_NO_ERR)
                printf("tlp_subscribe() failed, err: %d\n", p->err);
            else
                printf("ok\n");
            break;
        case PORT_SINK_PUSH:
            p->err = tlp_subscribe(
                apph,               /* 会话句柄 session handle   */
                &p->sh,             /* 订阅句柄 subscribe handle */
                NULL,               /* 用户引用 user ref         */
                NULL,               /* 回调函数 callback funktion */
                0u,                 /* 服务ID serviceId        */
                p->comid,           /* 端口号 comid            */
                0,                  /* 拓扑计数器 topo counter     */
                0,
                p->src,             /* 源地址 source address   */
                VOS_INADDR_ANY,
                p->dst,             /* 目标地址 destination address    */
                PORT_FLAGS,         /* 未设置标志 No flags set     */
                &comPrams,              /* 接收参数 Receive params */
                p->timeout,             /* 超时[usec] timeout [usec]   */
                TRDP_TO_SET_TO_ZERO);   /* 超时行为 timeout behavior */

            if (p->err != TRDP_NO_ERR)
                printf("tlp_subscribe() failed, err: %d\n", p->err);
            else
                printf("ok\n");
            break;
        }
    }
}

/* --- 将trdp错误代码转换为字符串 convert trdp error code to string -------------------------------------*/

static const char * get_result_string(int ret)
{
    static char buf[128];

    switch (ret)
    {
    case TRDP_NO_ERR:
        return "TRDP_NO_ERR (no error)";
    case TRDP_PARAM_ERR:
        return "TRDP_PARAM_ERR (parameter missing or out of range)";
    case TRDP_INIT_ERR:
        return "TRDP_INIT_ERR (call without valid initialization)";
    case TRDP_NOINIT_ERR:
        return "TRDP_NOINIT_ERR (call with invalid handle)";
    case TRDP_TIMEOUT_ERR:
        return "TRDP_TIMEOUT_ERR (timeout)";
    case TRDP_NODATA_ERR:
        return "TRDP_NODATA_ERR (non blocking mode: no data received)";
    case TRDP_SOCK_ERR:
        return "TRDP_SOCK_ERR (socket error / option not supported)";
    case TRDP_IO_ERR:
        return "TRDP_IO_ERR (socket IO error, data can't be received/sent)";
    case TRDP_MEM_ERR:
        return "TRDP_MEM_ERR (no more memory available)";
    case TRDP_SEMA_ERR:
        return "TRDP_SEMA_ERR semaphore not available)";
    case TRDP_QUEUE_ERR:
        return "TRDP_QUEUE_ERR (queue empty)";
    case TRDP_QUEUE_FULL_ERR:
        return "TRDP_QUEUE_FULL_ERR (queue full)";
    case TRDP_MUTEX_ERR:
        return "TRDP_MUTEX_ERR (mutex not available)";
    case TRDP_NOSESSION_ERR:
        return "TRDP_NOSESSION_ERR (no such session)";
    case TRDP_SESSION_ABORT_ERR:
        return "TRDP_SESSION_ABORT_ERR (Session aborted)";
    case TRDP_NOSUB_ERR:
        return "TRDP_NOSUB_ERR (no subscriber)";
    case TRDP_NOPUB_ERR:
        return "TRDP_NOPUB_ERR (no publisher)";
    case TRDP_NOLIST_ERR:
        return "TRDP_NOLIST_ERR (no listener)";
    case TRDP_CRC_ERR:
        return "TRDP_CRC_ERR (wrong CRC)";
    case TRDP_WIRE_ERR:
        return "TRDP_WIRE_ERR (wire error)";
    case TRDP_TOPO_ERR:
        return "TRDP_TOPO_ERR (invalid topo count)";
    case TRDP_COMID_ERR:
        return "TRDP_COMID_ERR (unknown comid)";
    case TRDP_STATE_ERR:
        return "TRDP_STATE_ERR (call in wrong state)";
    case TRDP_APP_TIMEOUT_ERR:
        return "TRDP_APPTIMEOUT_ERR (application timeout)";
    case TRDP_UNKNOWN_ERR:
        return "TRDP_UNKNOWN_ERR (unspecified error)";
    }
    sprintf(buf, "unknown error (%d)", ret);
    return buf;
}

/* --- 平台助手函数 platform helper functions ---------------------------------------------*/

#if (defined (WIN32) || defined (WIN64))

void cursor_home()
{
    COORD c = { 0, 0 };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

void clear_screen()
{
    CONSOLE_SCREEN_BUFFER_INFO ci;
    COORD c = { 0, 0 };
    DWORD written;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD a = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    cursor_home();
    SetConsoleTextAttribute(h, a);
    GetConsoleScreenBufferInfo(h, &ci);
    // fill attributes
    FillConsoleOutputAttribute(h, a, ci.dwSize.X * ci.dwSize.Y, c, &written);
    // fill characters
    FillConsoleOutputCharacter(h, ' ', ci.dwSize.X * ci.dwSize.Y, c, &written);
}

int _get_term_size(int * w, int * h)
{
    CONSOLE_SCREEN_BUFFER_INFO ci;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);
    if (w)
        *w = ci.dwSize.X;
    if (h)
        *h = ci.dwSize.Y;
    return 0;
}

void _set_color_red()
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_RED | FOREGROUND_INTENSITY);
}

void _set_color_green()
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_GREEN | FOREGROUND_INTENSITY);
}

void _set_color_blue()
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_BLUE | FOREGROUND_INTENSITY);
}

void _set_color_default()
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void _sleep_msec(int msec)
{
    Sleep(msec);
}

#if (!defined (WIN32) && !defined (WIN64))
int snprintf(char * str, size_t size, const char * format, ...)
{
    va_list args;
    int n;

    // verify buffer size
    if (size <= 0)
        // empty buffer
        return -1;

    // use vsnprintf function
    va_start(args, format);
    n = _vsnprintf(str, size, format, args);
    va_end(args);

    // check for truncated text
    if (n == -1 || n >= (int) size)
    {   // text truncated
        str[size - 1] = 0;
        return -1;
    }
    // return number of characters written to the buffer
    return n;
}
#endif
#elif defined (POSIX)

static void cursor_home()
{
    printf("\033" "[H");
}

static void clear_screen()
{
    printf("\033" "[H" "\033" "[2J");
}

static int _get_term_size(int * w, int * h)
{
    struct winsize ws;
    int ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    if (ret)
        return ret;
    if (ws.ws_col == 0)
        ws.ws_col = 120;
    if (ws.ws_row == 0)
        ws.ws_row = 40;
    if (w)
        *w = ws.ws_col;
    if (h)
        *h = ws.ws_row;
    return ret;
}

static void _set_color_red()
{
    printf("\033" "[0;1;31m");
}

static void _set_color_green()
{
    printf("\033" "[0;1;32m");
}

static void _set_color_blue()
{
    printf("\033" "[0;1;34m");
}

static void _set_color_default()
{
    printf("\033" "[0m");
}

static void _sleep_msec(int msec)
{
    struct timespec ts;
    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000L;
    // sleep specified time
    nanosleep(&ts, NULL);
}

#elif defined (VXWORKS)
void cursor_home()
{
    printf("\033" "[H");
}

void clear_screen()
{
    printf("\033" "[H" "\033" "[2J");
}

int _get_term_size(int * w, int * h)
{
    /* assume a terminal 100 cols, 60 rows fix */
    *w = 100;
    *h = 60;
    return 0;
}

void _set_color_red()
{
    printf("\033" "[0;1;31m");
}

void _set_color_green()
{
    printf("\033" "[0;1;32m");
}

void _set_color_blue()
{
    printf("\033" "[0;1;34m");
}

void _set_color_default()
{
    printf("\033" "[0m");
}

void _sleep_msec(int msec)
{
    struct timespec ts;
    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000L;
    /* sleep specified time*/
    nanosleep(&ts, NULL);
}

#else
#error "Target not defined!"
#endif

/* --- 试验数据处理 test data processing --------------------------------------------------*/

static void process_data()
{
    static int w = 80;
    int _w, _h;
    int i;
    unsigned n;
    TRDP_COM_PARAM_T comPrams = TRDP_PD_DEFAULT_SEND_PARAM;

    UINT8   data[1432u];
    // UINT8   data[6u];

    EGWM_TO_EDCU_SIGN1 sign1;
    EGWM_TO_EDCU_DOORTIME doortime;
    EGWM_TO_EDCU_DOORSEQUENCE doorsequence;

    sign1.terminalActive = 0x80;                    //TC1,TC2未占有
    sign1.centralDoorSign = 0x41;                   //集控开右门,限电模式开;
    doortime.delayTimeHBeforeClosing = 0x00;        //门关闭延时时间 
	doortime.delayTimeLBeforeClosing = 0x14;        // 20*0.1 = 2s
    doorsequence.closingAttemptsHAfterObstructionDetectionInClosingSequence = 0x00;    //关闭过程中障碍物探测的关闭次数 H
	doorsequence.closingAttemptsLAfterObstructionDetectionInClosingSequence = 0x03;    //关闭过程中障碍物探测的关闭次数 L
	
    data[0] = sign1.terminalActive;
    data[1] = sign1.centralDoorSign;
    data[2] = doortime.delayTimeHBeforeClosing;
    data[3] = doortime.delayTimeLBeforeClosing;
    data[4] = doorsequence.closingAttemptsHAfterObstructionDetectionInClosingSequence;
    data[5] = doorsequence.closingAttemptsLAfterObstructionDetectionInClosingSequence;

#if PORT_FLAGS == TRDP_FLAGS_TSN
    comPrams.vlan = 1;
    comPrams.tsn = TRUE;
#endif

    /* 获取终端大小 get terminal size */
    if (_get_term_size(&_w, &_h) == 0)
    {   /* 改变宽度？ changed width? */
        if (_w != w || !cycle)
            clear_screen();
        else
            cursor_home();
        w = _w;
    }
    else
    {
        if (!cycle)
            clear_screen();
        else
            cursor_home();
    }

    /* 逐个通过端口 */
    for (i = 0; i < nports; ++i)
    {
        Port * p = &ports[i];
        /* 写入端口数据 */
        if (p->type == PORT_PUSH || p->type == PORT_PULL)
        {
            if (p->link == -1)
            {   /* 数据发生器 */
                
                unsigned o = cycle % 128;
                memset(p->data, '_', p->size);
                // memset(p->data, data, p->size);
                for(int num = 0; num < 6; num++){
                    p->data[num] = data[num];
                }

                if (o < p->size)
                {
                    snprintf((char *) p->data + o, p->size - o,
                        "<Writing to %d port data...: %s/%d.%d.%d.%d->%d.%d.%d.%d/%dms/%db:%d,Data: %s,%s,%s,Door closed time is %.1f s,Detect time is %d>",
                        p->comid,
                        p->type == PORT_PUSH ? "Pd" : "Pp",
                        (p->src >> 24) & 0xff, (p->src >> 16) & 0xff,
                        (p->src >> 8) & 0xff, p->src & 0xff,
                        (p->dst >> 24) & 0xff, (p->dst >> 16) & 0xff,
                        (p->dst >> 8) & 0xff, p->dst & 0xff,
                        p->cycle / 1000, p->size, cycle,
                        data[0] && 0x80 != 0 ? "TC1 Open":"TC1 Closed",
                        data[0] && 0x40 != 0 ? "TC2 Used":"TC2 Unused",
                        data[1] && 0x40 != 0 ? "Left Door Open":"Left Door Closed",
                        (data[2]*16+data[3])*0.1,
                        data[4]*16+data[5] );
                }
            }
            else
            {   /* 从传入端口回显数据，用“~”替换所有“\” echo data from incoming port, replace all '_' by '~' */
                unsigned char * src = ports[p->link].data;
                unsigned char * dst = p->data;
                for (n = p->size; n; --n, ++src, ++dst)
                    *dst = (*src == '_') ? '~' : *src;
            }
#if PORT_FLAGS == TRDP_FLAGS_TSN
            if (p->type == PORT_PUSH)
            {
                p->err = tlp_putImmediate(apph, p->ph, p->data, p->size, 0);
            }
            else
            {
                p->err = tlp_put(apph, p->ph, p->data, p->size);
            }
#else
            p->err = tlp_put(apph, p->ph, p->data, p->size);
#endif
        }
        else if (p->type == PORT_REQUEST)
        {
            unsigned o = cycle % 128;
            memset(p->data, '_', p->size);
            // memset(p->data, data, p->size);
            for(int num = 0; num < 6; num++){
                p->data[num] = data[num];
            }
            if (o < p->size)
            {
                    snprintf((char *) p->data + o, p->size - o,
                        "<Writing to %d port data...: %s/%d.%d.%d.%d->%d.%d.%d.%d/%dms/%db:%d,Data: %s,%s,%s,Door closed time is %.1f s,Detect time is %d>",
                        p->comid,
                        p->type == PORT_PUSH ? "Pd" : "Pp",
                        (p->src >> 24) & 0xff, (p->src >> 16) & 0xff,
                        (p->src >> 8) & 0xff, p->src & 0xff,
                        (p->dst >> 24) & 0xff, (p->dst >> 16) & 0xff,
                        (p->dst >> 8) & 0xff, p->dst & 0xff,
                        p->cycle / 1000, p->size, cycle,
                        data[0] && 0x80 != 0 ? "TC1 Open":"TC1 Closed",
                        data[0] && 0x40 != 0 ? "TC2 Used":"TC2 Unused",
                        data[1] && 0x40 != 0 ? "Left Door Open":"Left Door Closed",
                        (data[2]*16+data[3])*0.1,
                        data[4]*16+data[5] );
            }

            p->err = tlp_request(apph, ports[p->link].sh, 0u, p->comid, 0u, 0u,
                p->src, p->dst, 0, PORT_FLAGS, &comPrams, p->data, p->size,
                p->repid, p->rep);
        }

        /* 打印端口数据 */
        fflush(stdout);
        if (vos_isMulticast(p->dst) || vos_isMulticast(p->src))
            _set_color_blue();
        else
            _set_color_default();
        printf("%5d ", p->comid);
        _set_color_default();
        printf("%s [", types[p->type]);

        if (p->err == TRDP_NO_ERR)
        {
            unsigned char * ptr = p->data;
            unsigned j;
            for (j = 0; j < (unsigned) w - 19; ++j, ++ptr)
            {
                if (j < p->size)
                {
                    if (*ptr < ' ' || *ptr > 127)
                        putchar('.');
                    else
                        putchar(*ptr);
                }
                else
                    putchar(' ');
            }
        }
        else
        {
            int n = printf(" -- %s", get_result_string(p->err));
            while (n++ < w - 19) putchar(' ');
        }
        putchar(']'); fflush(stdout);
        if (p->err != TRDP_NO_ERR)
            _set_color_red();
        else
            _set_color_green();
        printf(" %3d\n", p->err);
        _set_color_default();
    }
    /* 递增周期计数器  */
    ++cycle;
}

/* --- 轮询接收的数据 ----------------------------------------------------*/

static void poll_data()
{
    TRDP_PD_INFO_T pdi;
    int i;
    /* 逐个通过端口 */
    for (i = 0; i < nports; ++i)
    {
        Port * p = &ports[i];
        UINT32 size = p->size;
        if (p->type == PORT_SINK || p->type == PORT_SINK_PUSH)
        {
            p->err = tlp_get(apph, p->sh, &pdi, p->data, &size);
            snprintf((char *) p->data, p->size,
                        "<Read from %d port data: %s/%d.%d.%d.%d->%d.%d.%d.%d/%dms/%db:%d,open and closed times of door are %d>",
                        p->comid,
                        p->type == PORT_PUSH ? "Pd" : "Pp",
                        (p->src >> 24) & 0xff, (p->src >> 16) & 0xff,
                        (p->src >> 8) & 0xff, p->src & 0xff,
                        (p->dst >> 24) & 0xff, (p->dst >> 16) & 0xff,
                        (p->dst >> 8) & 0xff, p->dst & 0xff,
                        p->cycle / 1000, p->size, cycle,
                        p->data[0]*16 + p->data[1] );
        }        
    }
}


static FILE *pLogFile;

static void printLog(
    void        *pRefCon,
    VOS_LOG_T   category,
    const CHAR8 *pTime,
    const CHAR8 *pFile,
    UINT16      line,
    const CHAR8 *pMsgStr)
{
    if (pLogFile != NULL)
    {
        fprintf(pLogFile, "%s File: %s Line: %d %s\n", category==VOS_LOG_ERROR?"ERR ":(category==VOS_LOG_WARNING?"WAR ":(category==VOS_LOG_INFO?"INFO":"DBG ")), pFile, (int) line, pMsgStr);
        fflush(pLogFile);
    }
}

/* --- 主函数 ------------------------------------------------------------------*/
int main(int argc, char * argv[])
{
    TRDP_ERR_T err;
    unsigned tick = 0;

    printf("TRDP 组播程序：\n");

    if (argc < 4)
    {
        printf("usage: %s <localip> <remoteip> <mcast> <logfile>\n", argv[0]);
        printf("  <localip>  .. own IP address (ie. 10.2.24.1)\n");
        printf("  <remoteip> .. remote peer IP address (ie. 10.2.24.2)\n");
        printf("  <mcast>    .. multicast group address (ie. 239.2.24.1)\n");
        printf("  <logfile>  .. file name for logging (ie. test.txt)\n");
#ifdef SIM
        printf("  <prefix>  .. instance prefix in simulation mode (ie. CCU1)\n");
#endif
        return 1;
    }

    srcip = vos_dottedIP(argv[1]);
    dstip = vos_dottedIP(argv[2]);
    mcast = vos_dottedIP(argv[3]);

#ifdef SIM
    if (argc < 6)
    {
        printf("In simulation mode an extra last argument is required <Unike thread name>\n");
        return 1;
    }
    vos_setTimeSyncPrefix(argv[5]);

    if (!SimSetHostIp(argv[1]))
        printf("Failed to set sim host IP.");
#endif

    if (!srcip || !dstip || (mcast >> 28) != 0xE)
    {
        printf("invalid input arguments\n");
        return 1;
    }

    memset(&memcfg, 0, sizeof(memcfg));
    memset(&proccfg, 0, sizeof(proccfg));

    if (argc >= 5)
    {
        pLogFile = fopen(argv[4], "w+");
    }
    else
    {
        pLogFile = NULL;
    }

    /* 初始化TRDP协议库 */
    err = tlc_init(printLog, NULL, &memcfg);
    if (err != TRDP_NO_ERR)
    {
        printf("tlc_init() failed, err: %d\n", err);
        return 1;
    }
#ifdef SIM
    vos_threadRegister("main", TRUE);
#endif
    pdcfg.pfCbFunction = NULL;
    pdcfg.pRefCon = NULL;
    pdcfg.sendParam.qos = 5;
    pdcfg.sendParam.ttl = 64;
    pdcfg.flags = TRDP_FLAGS_NONE;
    pdcfg.timeout = 10000000;
    pdcfg.toBehavior = TRDP_TO_SET_TO_ZERO;
    pdcfg.port = 17224;

    /* 打开会话 */
    err = tlc_openSession(&apph, srcip, 0, NULL, &pdcfg, NULL, &proccfg);
    if (err != TRDP_NO_ERR)
    {
        printf("tlc_openSession() failed, err: %d\n", err);
        return 1;
    }

    /* 生成 EDCU 端口配置 */
    gen_push_ports_master(1104, 1040);
    // gen_push_ports_slave(1104, 1041);
    // gen_push_ports_slave(1104, 2040);
    // gen_push_ports_slave(1104, 2041);
    // gen_push_ports_slave(1104, 3040);
    // gen_push_ports_slave(1104, 3041);

    setup_ports();
    vos_threadDelay(2000000);
    /* 主测试循环 */
    while (1)
    {   /* 驱动TRDP通信 */
        tlc_process(apph, NULL, NULL);
        /* 轮询（接收）数据 */
        poll_data();
        /* 每500毫秒处理一次数据 */
        if (!(++tick % 50))
            process_data();
        /* 等待10毫秒 */
        vos_threadDelay(10000);
    }

    return 0;
}

/* ---------------------------------------------------------------------------*/
