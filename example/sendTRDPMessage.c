/******************************************************************************/
/** 文件名   sendTRDPMessage.c                                                    **/
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
#include "vos_thread.h"
#include "vos_utils.h"
#include "trdpProc.h"


/***********************************************************************************************************************
 * 宏定义
 */
#define APP_VERSION     "1.4"

#define DATA_MAX        1432u

#define PD_COMID        0u
#define PD_COMID_CYCLE  1000000u             /* in us (1000000 = 1 sec) */

/* 动态内存   */
#define RESERVED_MEMORY  160000u



/***********************************************************************************************************************
 * 函数原型
 */
void dbgOut (void *,
             TRDP_LOG_T,
             const  CHAR8 *,
             const  CHAR8 *,
             UINT16,
             const  CHAR8 *);
void    usage (const char *);
void    myPDcallBack (void *,
                      TRDP_APP_SESSION_T,
                      const TRDP_PD_INFO_T *,
                      UINT8 *,
                      UINT32 );

/**********************************************************************************************************************/
/** TRDP日志记录/错误输出的回调例程——参数说明
 *
 *  @param[in]      pRefCon             用户提供的上下文指针
 *  @param[in]      category            日志类别（错误、警告、信息等）
 *  @param[in]      pTime               指向以 NULL 结尾的时间戳字符串的指针
 *  @param[in]      pFile               指向以 NULL 结尾的源模块字符串的指针
 *  @param[in]      LineNumber          线
 *  @param[in]      pMsgStr             指向以 NULL 结尾的字符串的指针
 *
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
    printf("%s %s %s:%d %s",
           strrchr(pTime, '-') + 1,
           catStr[category],
           (pF == NULL)? "" : pF + 1,
           LineNumber,
           pMsgStr);
}

/* 打印合理函数用法信息 */
void usage (const char *appName)
{
    printf("Usage of %s\n", appName);
    printf("This tool sends PD messages to an ED.\n"
           "Arguments are:\n"
           "-o <own IP address> (default INADDR_ANY)\n"
           "-t <target IP address>\n"
           "-c <comId> (default 0)\n"
           "-s <cycle time> (default 1000000 [us])\n"
           "-e send empty request\n"
           "-d <custom string to send> (default: 'Hello World')\n"
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
    unsigned int            ip[4];
    INT32                   hugeCounter = 0;
    TRDP_APP_SESSION_T      appHandle; /*    库实例的标识符    */
    TRDP_PUB_T              pubHandle; /*    自定义的标识符         */
    UINT32                  comId           = PD_COMID;
    UINT32                  interval        = PD_COMID_CYCLE;
    TRDP_ERR_T              err;
    TRDP_PD_CONFIG_T        pdConfiguration =
    {NULL, NULL, TRDP_PD_DEFAULT_SEND_PARAM, TRDP_FLAGS_NONE, 1000000u, TRDP_TO_SET_TO_ZERO, 0};
    TRDP_MEM_CONFIG_T       dynamicConfig   = {NULL, RESERVED_MEMORY, {0}};
    TRDP_PROCESS_CONFIG_T   processConfig   = {"Me", "", TRDP_PROCESS_DEFAULT_CYCLE_TIME, 0u, TRDP_OPTION_BLOCK};
    UINT32                  ownIP           = 0u;
    int                     rv = 0;
    UINT32                  destIP = 0u;

    /*    当没有指定时，生成一些想发送数据   */
    UINT8                   *outputBuffer;
    //UINT8                   exampleData[DATA_MAX]   = "Hello World";
    UINT32                  outputBufferSize        = 24u;

    UINT8                   data[DATA_MAX];
    int ch;

    PACKET_HEAD date;
    date.year = 0x07E4;
    date.month = 0x08;
    date.day = 0x08;

    EGWM_TO_EDCU_SIGN1 sign1;
    EGWM_TO_EDCU_DOORTIME doortime;
    EGWM_TO_EDCU_DOORSEQUENCE doorsequence;

    sign1.terminalActive = 0x80;                    //TC1,TC2未占有
	sign1.centralDoorSign = 0x41;                   //集控开右门,限电模式开;
    doortime.delayTimeHBeforeClosing = 0x00;        //门关闭延时时间 
	doortime.delayTimeLBeforeClosing = 0x14;        // 20*0.1 = 2s
	doortime.delayTimeHBeforeOpening = 0x00;        //门打开延时时间 
	doortime.delayTimeLBeforeOpening = 0x32;        // 50*0.1 = 5s
	doortime.obstructionDetectionDelayTimeH = 0x00; //障碍物检测延时时间 H
	doortime.obstructionDetectionDelayTimeL = 0x0A; // 1s
	doortime.doorOpenTimeH = 0x00;                  //开门过程持续时间 H 
    doortime.doorOpenTimeL = 0x1E;                  // 3s 
	doortime.doorCloseTimeH = 0x00;                 //关门过程持续时间 H 
	doortime.doorCloseTimeL = 0x1E;                 //关门过程持续时间 L
    doorsequence.closingAttemptsHAfterObstructionDetectionInClosingSequence = 0x00;    //关闭过程中障碍物探测的关闭次数 H
	doorsequence.closingAttemptsLAfterObstructionDetectionInClosingSequence = 0x03;    //关闭过程中障碍物探测的关闭次数 L
	doorsequence.openingAttemptsHAfterObstructionDetectionInOpeningSequence = 0x00;    //开启过程中障碍物探测的开启次数 H    
	doorsequence.openingAttemptsLAfterObstructionDetectionInOpeningSequence = 0x02;    //开启过程中障碍物探测的开启次数 L    
	doorsequence.reopenDistanceHAfterObstructionDetectionInClosingSequence = 0x00;     //关闭过程中障碍物探测的重新开启距离 H      
	doorsequence.reopenDistanceLAfterObstructionDetectionInClosingSequence = 0x03;     //关闭过程中障碍物探测的重新开启距离 L  

    int i;
    UINT8 *p1 = (UINT8*)(&sign1);
    for (i = 0; i < sizeof(sign1)/sizeof(UINT8); i++)
    {
        data[i] = *p1;
        p1++;
    }
    UINT8 *p2 = (UINT8*)(&doortime);
    for (; i < sizeof(sign1)/sizeof(UINT8) + sizeof(doortime)/sizeof(UINT8); i++)
    {
        data[i] = *p2;
        p2++;
    }
    UINT8 *p3 = (UINT8*)(&doorsequence);
    for (; i < sizeof(sign1)/sizeof(UINT8) + sizeof(doortime)/sizeof(UINT8) + sizeof(doorsequence)/sizeof(UINT8); i++)
    {
        data[i] = *p3;
        p3++;
    }
    //outputBuffer = exampleData;
    outputBuffer = data;
    outputBufferSize = sizeof(sign1)+sizeof(doortime)+sizeof(doorsequence);

    if (argc <= 1)
    {
        usage(argv[0]);
        return 1;
    }

    while ((ch = getopt(argc, argv, "t:o:d:s:h?vec:")) != -1)
    {
        switch (ch)
        {
           case 'o':
           {    /*  read ip    */
               if (sscanf(optarg, "%u.%u.%u.%u",
                          &ip[3], &ip[2], &ip[1], &ip[0]) < 4)
               {
                   usage(argv[0]);
                   exit(1);
               }
               ownIP = (ip[3] << 24) | (ip[2] << 16) | (ip[1] << 8) | ip[0];
               break;
           }
           case 'c':
           {    /*  read comId    */
               if (sscanf(optarg, "%u",
                          &comId) < 1)
               {
                   usage(argv[0]);
                   exit(1);
               }
               break;
           }
           case 's':
           {    /*  read cycle time    */
               if (sscanf(optarg, "%u",
                          &interval) < 1)
               {
                   usage(argv[0]);
                   exit(1);
               }
               break;
           }
           case 't':
           {    /*  read ip    */
               if (sscanf(optarg, "%u.%u.%u.%u",
                          &ip[3], &ip[2], &ip[1], &ip[0]) < 4)
               {
                   usage(argv[0]);
                   exit(1);
               }
               destIP = (ip[3] << 24) | (ip[2] << 16) | (ip[1] << 8) | ip[0];
               break;
           }
           case 'e':
           {
               outputBuffer     = NULL;
               outputBufferSize = 0;
           }
           break;
           case 'd':
           {    /*  data   此处填充数据 */
               char     c;
               UINT32   dataSize = 0u;
               do
               {
                   c = optarg[dataSize];
                   dataSize++;
               }
               while (c != '\0');

               if (dataSize >= DATA_MAX)
               {
                   fprintf(stderr, "The data is too long\n");
                   return 1;
               }
               memcpy(data, optarg, dataSize);
               outputBuffer     = data;     /* 将指针移到新数据 */
               outputBufferSize = dataSize;
               break;
           }
           case 'v':    /*  version */
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

    if (destIP == 0)
    {
        fprintf(stderr, "No destination address given!\n");
        usage(argv[0]);
        return 1;
    }

    /*    Init the library  */
    if (tlc_init(&dbgOut,                              /* 无日志记录    */
                 NULL,
                 &dynamicConfig) != TRDP_NO_ERR)    /* 使用应用程序提供的内存    */
    {
        printf("Initialization error\n");
        return 1;
    }

    /*    Open a session  */
    if (tlc_openSession(&appHandle,
                        ownIP, 0,               /* 使用默认IP地址          */
                        NULL,                   /* 禁止编组                 */
                        &pdConfiguration, NULL, /* PD和MD的系统默认值  */
                        &processConfig) != TRDP_NO_ERR)
    {
        vos_printLogStr(VOS_LOG_USR, "Initialization error\n");
        return 1;
    }

    /*    将数据包复制到内部发送队列中，准备发送    */
    /*    如果我们改变了数据，就重新发布它         */
    err = tlp_publish(  appHandle,                  /*    我们的应用程序标识符             */
                        &pubHandle,                 /*    我们的脉冲标识符                 */
                        NULL, NULL,
                        0u,
                        comId,                      /*    要发送的端口号                   */
                        0u,                         /*    etbTopoCnt = 0，仅适用于本地组   */
                        0u,                         /*    opTopoCnt = 0，对于非直接数据    */
                        ownIP,                      /*    默认源IP                        */
                        destIP,                     /*    目标地址IP                      */
                        interval,                   /*    循环周期                        */
                        0u,                         /*    无多余                          */
                        TRDP_FLAGS_NONE,            /*    对错误使用回调                   */
                        NULL,                       /*    默认qos和ttl                    */
                        (UINT8 *)outputBuffer,      /*    初始数据                        */
                        outputBufferSize            /*    数据大小                        */
                        );


    if (err != TRDP_NO_ERR)
    {
        vos_printLog(VOS_LOG_USR, "tlp_publish error (%s)\n", vos_getErrorString((VOS_ERR_T)err));
        tlc_terminate();
        return 1;
    }

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
        TRDP_FDS_T          rfds;
        INT32               noDesc;
        TRDP_TIME_T         tv;
        const TRDP_TIME_T   max_tv  = {0, 1000000};
        const TRDP_TIME_T   min_tv  = {0, TRDP_PROCESS_DEFAULT_CYCLE_TIME};

        /*
            为select调用准备文件描述符集。
            可以在此处添加其他描述符。
         */
        FD_ZERO(&rfds);
        /* FD_SET(pd_fd, &rfds); */

        /*
            计算select的最小超时值。
            这样我们可以保证PDs及时发送。
            以最小的CPU负载和最小的抖动。
         */
        tlc_getInterval(appHandle, &tv, &rfds, &noDesc);

        /*
            select的等待时间必须考虑接收或发送的PD数据包的周期时间和超时。
            如果我们需要比最低PD周期更快，我们需要给自己设定最长的时间。
         */
        if (vos_cmpTime(&tv, &max_tv) > 0)
        {
            tv = max_tv;
        }
        else if (vos_cmpTime(&tv, &min_tv) < 0)
        {
            tv = min_tv;
        }

        /*
          Select（）将等待就绪描述符或超时，别的事先发生。
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

        /*  处理其他就绪描述符 */
        if (rv > 0)
        {
            vos_printLogStr(VOS_LOG_USR, "other descriptors were ready\n");
        }
        else
        {
            fprintf(stdout, ".");
            fflush(stdout);
        }

        if (outputBuffer != NULL && strlen((char *)outputBuffer) != 0)
        {
            sprintf((char *)outputBuffer, "Just a Counter: %08d", hugeCounter++);
            outputBufferSize = (UINT32) strlen((char *)outputBuffer);
        }

        err = tlp_put(appHandle, pubHandle, outputBuffer, outputBufferSize);
        if (err != TRDP_NO_ERR)
        {
            vos_printLogStr(VOS_LOG_ERROR, "put pd error\n");
            rv = 1;
            break;
        }
    }

    /*
     *     清理缓存
     */
    tlp_unpublish(appHandle, pubHandle);
    tlc_closeSession(appHandle);
    tlc_terminate();
    return rv;
}
