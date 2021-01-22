/******************************************************************************/
/** 文件名   receiveTRDPMessage.c                                                    **/
/** 作  者   王浩洋                                                          **/
/** 版  本   V1.0.0                                                          **/
/** 日  期   2020 12                                                 **/
/******************************************************************************/

/***********************************************************************************************************************
 * 引用库函数
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined (POSIX)
#include <unistd.h>
#include <sys/select.h>
#elif (defined (WIN32) || defined (WIN64))
#include "getopt.h"
#endif

#include "trdp_if_light.h"
#include "tau_marshall.h"
#include "vos_utils.h"
#include "trdpProc.h"

/***********************************************************************************************************************
 * 宏定义
 */
#define APP_VERSION     "1.4"

#define DATA_MAX        1432

#define PD_COMID        0
#define PD_COMID_CYCLE  1000000             /* in us (1000000 = 1 sec) */

/* 动态内存 */
#define RESERVED_MEMORY  1000000

//CHAR8 gBuffer[32];
unsigned char gBuffer[DATA_MAX];

/***********************************************************************************************************************
 * 接收处理
 */
char * terminalActiveprocess(char g)
{
    char message[50]; 
    int i = g && 0x80, j = g && 0x40;
    if(i == 1 && j == 1){
        strcpy(message,"11");
        return message;
    }else if(i == 1){
        strcpy(message,"10");
        return message;
    }else if(j == 1){
        strcpy(message,"01");
        return message;
    }
    strcpy(message,"00");
    return message;  
}

/***********************************************************************************************************************
 * 函数原型
 */
void dbgOut (void *,
             TRDP_LOG_T,
             const  CHAR8 *,
             const  CHAR8 *,
             UINT16,
             const  CHAR8 *);
void usage (const char *);

/**********************************************************************************************************************/
/**  TRDP日志记录/错误输出的回调例程——参数说明
 *  
 *  @param[in]        pRefCon          用户提供的上下文指针
 *  @param[in]        category         日志类别（错误、警告、信息等）
 *  @param[in]        pTime            指向以 NULL 结尾的时间戳字符串的指针
 *  @param[in]        pFile            指向以 NULL 结尾的源模块字符串的指针
 *  @param[in]        LineNumber       线
 *  @param[in]        pMsgStr          指向以 NULL 结尾的字符串的指针
 *  @retval         none
 */
void dbgOut (
    void        *pRefCon,
    TRDP_LOG_T  category,
    const CHAR8 *pTime,
    const CHAR8 *pFile,
    UINT16      LineNumber,
    const CHAR8 *pMsgStr)
{
    const char *catStr[] = {"**Error:", "Warning:", "   Info:", "  Debug:", "   User:"};
    CHAR8       *pF = strrchr(pFile, VOS_DIR_SEP);

    if (category != VOS_LOG_DBG)
    {
        printf("%s %s %s:%d %s",
               pTime,
               catStr[category],
               (pF == NULL)? "" : pF + 1,
               LineNumber,
               pMsgStr);
    }
}

/* 打印合理函数用法信息 */
void usage (const char *appName)
{
    printf("Usage of %s\n", appName);
    printf("This tool receives PD messages from an ED.\n"
           "Arguments are:\n"
           "-o <own IP address> (default: default interface)\n"
           "-m <multicast group IP> (default: none)\n"
           "-c <comId> (default 0)\n"
           "-v print version and quit\n"
           );
}


/**********************************************************************************************************************/
/** 主函数入口——返回值说明
 *
 *  @retval         0        没有错误
 *  @retval         1        有错误
 */
int main (int argc, char *argv[])
{
    unsigned int    ip[4];
    TRDP_APP_SESSION_T      appHandle;    /*   库实例的标识符    */
    TRDP_SUB_T              subHandle;    /*   自定义的标识符      */
    UINT32          comId = PD_COMID;
    TRDP_ERR_T err;
    TRDP_PD_CONFIG_T        pdConfiguration =
    {NULL, NULL, TRDP_PD_DEFAULT_SEND_PARAM, TRDP_FLAGS_NONE, 1000000u, TRDP_TO_SET_TO_ZERO, 0u};
    TRDP_MEM_CONFIG_T       dynamicConfig   = {NULL, RESERVED_MEMORY, {0}};
    TRDP_PROCESS_CONFIG_T   processConfig   = {"Me", "", TRDP_PROCESS_DEFAULT_CYCLE_TIME, 0, TRDP_OPTION_NONE};
    UINT32  ownIP   = 0u;
    UINT32  dstIP   = 0u;
    int     rv      = 0;

    int     ch;
    TRDP_PD_INFO_T myPDInfo;
    UINT32  receivedSize;

    while ((ch = getopt(argc, argv, "o:m:h?vc:")) != -1)
    {
        switch (ch)
        {
           case 'o':
           {    /*  读取IP  */
               if (sscanf(optarg, "%u.%u.%u.%u",
                          &ip[3], &ip[2], &ip[1], &ip[0]) < 4)
               {
                   usage(argv[0]);
                   exit(1);
               }
               ownIP = (ip[3] << 24) | (ip[2] << 16) | (ip[1] << 8) | ip[0];
               break;
           }
           case 'm':
           {    /*  读取IP  */
               if (sscanf(optarg, "%u.%u.%u.%u",
                          &ip[3], &ip[2], &ip[1], &ip[0]) < 4)
               {
                   usage(argv[0]);
                   exit(1);
               }
               dstIP = (ip[3] << 24) | (ip[2] << 16) | (ip[1] << 8) | ip[0];
               break;
           }vos_printLog(VOS_LOG_USR, "%s\n", gBuffer);
           case 'c':
           {    /*  读取端口号  */
               if (sscanf(optarg, "%u",
                          &comId) < 1)
               {
                   usage(argv[0]);
                   exit(1);
               }
               break;
           }
           case 'v':    /*  版本 */
               printf("%s: Version %s\t(%s - %s)\n",
                      argv[0], APP_VERSION, __DATE__, __TIME__);
               exit(0);
               break;
           case 'h':
           case '?':
           default:
               usage(argv[0]);
               return 1;
        }
    }

    /*   初始化库  */
    if (tlc_init(&dbgOut,                              /*  无日志记录   */
                 NULL,
                 &dynamicConfig) != TRDP_NO_ERR)    /* 使用应用程序提供的内存  */
    {
        printf("Initialization error\n");
        return 1;
    }

    /*   打开一个会话  */
    if (tlc_openSession(&appHandle,
                        ownIP, 0,               /*   使用默认IP地址        */
                        NULL,                   /*   禁止编组              */
                        &pdConfiguration, NULL, /*   PD和MD的系统默认值     */
                        &processConfig) != TRDP_NO_ERR)
    {
        vos_printLogStr(VOS_LOG_USR, "Initialization error\n");
        return 1;
    }

    /*    订阅控制PD       */

    memset(gBuffer, 0, sizeof(gBuffer));

    err = tlp_subscribe( appHandle,                 /*    自定义的应用程序标识符      */
                         &subHandle,                /*    自定义的订阅标识符          */
                         NULL,                      /*    用户参考号                 */
                         NULL,                      /*    回调函数                   */
                         0u,
                         comId,                     /*    端口号                     */
                         0,                         /*    etbTopoCnt：仅本地组        */
                         0,                         /*    opTopoCnt                  */
                         VOS_INADDR_ANY, VOS_INADDR_ANY,    /*    源IP筛选器          */
                         dstIP,                     /*    默认目的地（或MC组）         */
                         TRDP_FLAGS_DEFAULT,        /*    TRDP标志                    */
                         NULL,                      /*    默认接口                    */
                         PD_COMID_CYCLE * 3,        /*    超时                        */
                         TRDP_TO_SET_TO_ZERO        /*    超时时删除无效数据           */
                         );

    if (err != TRDP_NO_ERR)
    {
        vos_printLogStr(VOS_LOG_ERROR, "prep pd receive error\n");
        tlc_terminate();
        return 1;
    }

    /*
        完成设置。
        对于非高性能目标，这是不可行的。
        如果定义了 HIGH_PERF_INDEXED，则此调用是必需的。它将创建内部索引表，以便更快地访问。
        它应该在添加最后一个发布者和订阅者之后调用。
        也许 tlc_activateSession 可能会有个更好的名字。如果设置了 HIGH_PERF_INDEXED，此调用将创建内部索引表，以便快速访问电报。
     */

    err = tlc_updateSession(appHandle);
    if (err != TRDP_NO_ERR)
    {
        vos_printLog(VOS_LOG_USR, "tlc_updateSession error (%s)\n", vos_getErrorString((VOS_ERR_T)err));
        tlc_terminate();
        return 1;
    }

    /*
        进入主处理循环
     */
    while (1)
    {
        TRDP_FDS_T rfds;
        INT32 noDesc;
        TRDP_TIME_T tv = {0, 0};
        const TRDP_TIME_T   max_tv  = {1, 0};
        const TRDP_TIME_T   min_tv  = {0, TRDP_PROCESS_DEFAULT_CYCLE_TIME};

        /*
            为select调用准备文件描述符集。
            可以在此处添加其他描述符。
         */
        FD_ZERO(&rfds);

        /*
            计算select的最小超时值。
            这样我们可以保证PDs及时发送。
            以最小的CPU负载和最小的抖动。
         */
        tlc_getInterval(appHandle, &tv, &rfds, &noDesc);

        /*
            select的等待时间必须考虑
            接收或发送的PD数据包。
            如果我们需要比最低PD周期更快的投票，
            我们需要给自己设定最长的时间。
         */
        if (vos_cmpTime(&tv, &max_tv) > 0)
        {
            tv = max_tv;
        }

        /*
            防止运行太快，如果我们只是等待数据包（默认最小时间是10毫秒）。
        */
        if (vos_cmpTime(&tv, &min_tv) < 0)
        {
            tv = min_tv;
        }

        /*
            或者，我们可以使用空指针调用select（），这将阻止此循环：
            if (vos_cmpTime(&tv, &min_tv) < 0)
            {
                rv = vos_select(noDesc + 1, &rfds, NULL, NULL, NULL);
            }
        */

        /*
            select（）将等待就绪描述符或超时，
            什么事先发生。
         */
        rv = vos_select(noDesc + 1, &rfds, NULL, NULL, &tv);

        /*
            检查过期的PDs（发送和接收）
            如果是过期的，发送任何挂起的PDs...
            检测丢失的PDs...
            “rv”将更新以显示已处理的事件（如果有不止一个...）
            回调函数将从 tlc_process 函数中调用（在它的上下文和线程中）
         */
        (void) tlc_process(appHandle, &rfds, &rv);

        /* 处理其他就绪描述符... */
        if (rv > 0)
        {
            vos_printLogStr(VOS_LOG_USR, "other descriptors were ready\n");
        }
        else
        {
            /*printf(".");
            fflush(stdout);*/
        }

        /*
            收到订阅的电报。
            唯一受支持的数据包标志是TRDP_FLAGS_MARSHALL，它将自动对电报进行 de-marshall 处理。
            这里我们不用。
         */

        receivedSize = sizeof(gBuffer);
        err = tlp_get(appHandle,
                      subHandle,
                      &myPDInfo,
                      (unsigned char *) gBuffer,
                      &receivedSize);
        if ((TRDP_NO_ERR == err)
            && (receivedSize > 0))
        {
            vos_printLogStr(VOS_LOG_USR, "\n\nMessage reveived:\n");
            vos_printLog(VOS_LOG_USR, "Type = %c%c, \n", myPDInfo.msgType >> 8, myPDInfo.msgType & 0xFF);
            vos_printLog(VOS_LOG_USR, "Seq  = %u, \n", myPDInfo.seqCount);
            vos_printLog(VOS_LOG_USR, "with %d Bytes:\n", receivedSize);
            /*vos_printLog(VOS_LOG_USR, "gBuffer0-7:   %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx\n",
                   gBuffer[0], gBuffer[1], gBuffer[2], gBuffer[3],
                   gBuffer[4], gBuffer[5], gBuffer[6], gBuffer[7]);
            vos_printLog(VOS_LOG_USR, "gBuffer8-15:   %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx\n",
                   gBuffer[8], gBuffer[9], gBuffer[10], gBuffer[11],
                   gBuffer[12], gBuffer[13], gBuffer[14], gBuffer[15]);*/
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[0]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[1]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[2]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[3]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[4]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[5]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[6]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[7]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[8]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[9]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[10]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[11]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[12]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[13]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[14]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[15]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[16]);
            vos_printLog(VOS_LOG_USR, "gBuffer0:   %02hhx\n",gBuffer[17]);
            //vos_printLog(VOS_LOG_USR, "gBuffer0:   %d\n",gBuffer[0] && 0x80);
            //vos_printLog(VOS_LOG_USR, "gBuffer0:   %d\n",gBuffer[0] && 0x40);
            //vos_printLog(VOS_LOG_USR, "gBuffer0:   %d\n",gBuffer[0] - 0x80);
            //vos_printLog(VOS_LOG_USR, "gBuffer0:   %d\n",gBuffer[0] - 0x40);
            vos_printLog(VOS_LOG_USR, "\n");
            vos_printLog(VOS_LOG_USR, "gBuffer0:  %02hhx\n",gBuffer[0] & 0x80);
            vos_printLog(VOS_LOG_USR, "gBuffer0:  %02hhx\n",gBuffer[0] & 0x40);
            vos_printLog(VOS_LOG_USR, "gBuffer0:  %02hhx\n",gBuffer[1] & 0x20);
            vos_printLog(VOS_LOG_USR, "gBuffer0:  %02hhx\n",gBuffer[1] & 0x08);
            vos_printLog(VOS_LOG_USR, "gBuffer0:  %02hhx\n",gBuffer[1] & 0x04);
            vos_printLog(VOS_LOG_USR, "gBuffer0:  %02hhx\n",gBuffer[1] & 0x01);

            char tmp1 =  gBuffer[0];
            if(gBuffer[0] & 0x80 != 0) vos_printLogStr(VOS_LOG_USR,"TC1占有\n");
            if(gBuffer[0] & 0x40 != 0) vos_printLogStr(VOS_LOG_USR,"TC2占有\n");
            if(gBuffer[1] & 0x40 != 0) vos_printLogStr(VOS_LOG_USR,"集控开左门\n");
            if(gBuffer[1] & 0x20 != 0) vos_printLogStr(VOS_LOG_USR,"集控关左门\n");
            if(gBuffer[1] & 0x08 != 0) vos_printLogStr(VOS_LOG_USR,"集控开右门\n");
            if(gBuffer[1] & 0x04 != 0) vos_printLogStr(VOS_LOG_USR,"集控关右门\n");
            if(gBuffer[1] & 0x01 != 0) vos_printLogStr(VOS_LOG_USR,"限电模式开\n");
            vos_printLog(VOS_LOG_USR, "门关闭延时时间为 %.1f s\n", (gBuffer[2]*16+gBuffer[3])*0.1);
            vos_printLog(VOS_LOG_USR, "门打开延时时间为 %.1f s\n", (gBuffer[4]*16+gBuffer[5])*0.1);
            vos_printLog(VOS_LOG_USR, "障碍物检测延时时间为 %.1f s\n", (gBuffer[6]*16+gBuffer[7])*0.1);
            vos_printLog(VOS_LOG_USR, "开门过程持续时间为 %.1f s\n", (gBuffer[8]*16+gBuffer[9])*0.1);
            vos_printLog(VOS_LOG_USR, "关门过程持续时间为 %.1f s\n", (gBuffer[10]*16+gBuffer[11])*0.1);

            vos_printLog(VOS_LOG_USR, "关闭过程中障碍物探测的关闭次数为 %d 次\n", (gBuffer[12]*16+gBuffer[13]));
            vos_printLog(VOS_LOG_USR, "开启过程中障碍物探测的开启次数为 %d 次\n", (gBuffer[14]*16+gBuffer[15]));
            vos_printLog(VOS_LOG_USR, "关闭过程中障碍物探测的重新开启距离为 %.1f mm\n", (gBuffer[16]*16+gBuffer[17])*100);
        
            //vos_printLogStr(VOS_LOG_USR,message0);
            //vos_printLog(VOS_LOG_USR, "%c\n", terminalActiveprocess(gBuffer[0]));
            vos_printLog(VOS_LOG_USR, "%s\n", gBuffer);
        }
        else if (TRDP_NO_ERR == err)
        {
            vos_printLogStr(VOS_LOG_USR, "\nMessage reveived:\n");
            vos_printLog(VOS_LOG_USR, "Type = %c%c - \n", myPDInfo.msgType >> 8, myPDInfo.msgType & 0xFF);
            vos_printLog(VOS_LOG_USR, "Seq  = %u\n", myPDInfo.seqCount);
        }
        else if (TRDP_TIMEOUT_ERR == err)
        {
            vos_printLogStr(VOS_LOG_INFO, "Packet timed out\n");
        }
        else if (TRDP_NODATA_ERR == err)
        {
            vos_printLogStr(VOS_LOG_INFO, "No data yet\n");
        }
        else
        {
            vos_printLog(VOS_LOG_ERROR, "PD GET ERROR: %d\n", err);
        }
    }

    /*
     *   清理缓存
     */
    tlp_unsubscribe(appHandle, subHandle);
    tlc_closeSession(appHandle);
    tlc_terminate();
    return rv;
}
