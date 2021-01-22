/**********************************************************************************************************************/
/**
 * @file            pd_responder.c
 *
 * @brief           Test application for TRDP
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Bernd Loehr, NewTec GmbH
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright Bombardier Transportation Inc. or its subsidiaries and others, 2013. All rights reserved.
 *
 * $Id: pd_responder.c 2183 2020-07-29 15:51:09Z bloehr $
 *
 *      BL 2020-07-29: Renamed (no MD support)
 *      BL 2017-06-30: Compiler warnings, local prototypes added
 */

/***********************************************************************************************************************
 * INCLUDES
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

/***********************************************************************************************************************
 * DEFINES
 */

/* Some sample comId definitions    */
#define PUBLISH_COMID_BASE      2000
#define SUBSCRIBE_COMID_BASE    2000
#define PUBLISH_INTERVAL_BASE   10000
#define SUBSCRIBE_TIMEOUT_BASE  10000000
#define PUBLISH_ADDR_BASE       0x0a40f000
#define SUBSCRIBE_ADDR_BASE     0x0a40f000
#define DATASIZE_STEP           120

/* We use dynamic memory    */
#define RESERVED_MEMORY     1000000

#define APP_VERSION         "0.1"

#define MAX_NO_OF_PKTS      10

typedef struct pd_demo_pkt
{
    TRDP_SUB_T  subHandle;          /*  Our identifier to the subscription        */
    TRDP_PUB_T  pubHandle;          /*  Our identifier to the publication         */
    UINT32      comID;              /*  comID == dataSetID  */
    UINT32      time;               /*  us interval or time out */
    UINT32      addr;               /*  dest addr */
    UINT32      dataSize;           /*  actual data size  */
    UINT8       data[TRDP_MAX_PD_DATA_SIZE];
} PD_PKT_T;

PD_PKT_T    gPubPackets[MAX_NO_OF_PKTS];
PD_PKT_T    gSubPackets[MAX_NO_OF_PKTS];

UINT32      gOwnIP  = 0u;
UINT32      gDestIP = 0xEF000000u;

const UINT8 cDemoData[] = " "
    "Far out in the uncharted backwaters of the unfashionable end of the western spiral arm of the Galaxy lies a small unregarded yellow sun. Orbiting this at a distance of roughly ninety-two million miles is an utterly insignificant little blue green planet whose ape-descended life forms are so amazingly primitive that they still think digital watches are a pretty neat idea.\n"
    "This planet has – or rather had – a problem, which was this: most of the people on it were unhappy for pretty much of the time. Many solutions were suggested for this problem, but most of these were largely concerned with the movements of small green pieces of paper, which is odd because on the whole it wasn’t the small green pieces of paper that were unhappy.\n"
    "And so the problem remained; lots of the people were mean, and most of them were miserable, even the ones with digital watches.\n"
    "Many were increasingly of the opinion that they’d all made a big mistake in coming down from the trees in the first place. And some said that even the trees had been a bad move, and that no one should ever have left the oceans.\n"
    "And then, one Thursday, nearly two thousand years after one man had been nailed to a tree for saying how great it would be to be nice to people for a change, one girl sitting on her own in a small cafe in Rickmansworth suddenly realized what it was that had been going wrong all this time, and she finally knew how the world could be made a good and happy place. This time it was right, it would work, and no one would have to get nailed to anything.\n"
    "Sadly, however, before she could get to a phone to tell anyone about it, a terribly stupid catastrophe occurred, and the idea was lost forever.\n"
    "This is not her story.\n"
    "But it is the story of that terrible stupid catastrophe and some of its consequences.\n"
    "It is also the story of a book, a book called The Hitchhiker’s Guide to the Galaxy – not an Earth book, never published on Earth, and until the terrible catastrophe occurred, never seen or heard of by any Earthman.\n"
    "Nevertheless, a wholly remarkable book.\n"
    "In fact it was probably the most remarkable book ever to come out of the great publishing houses of Ursa Minor – of which no Earthman had ever heard either.\n"
    "Not only is it a wholly remarkable book, it is also a highly successful one – more popular than the Celestial Home Care Omnibus, better selling than Fifty More Things to do in Zero Gravity, and more controversial than Oolon Colluphid’s trilogy of philosophical blockbusters Where God Went Wrong, Some More of God’s Greatest Mistakes and Who is this God Person Anyway?\n"
    "In many of the more relaxed civilizations on the Outer Eastern Rim of the Galaxy, the Hitchhiker’s Guide has already supplanted the great Encyclopedia Galactica as the standard repository of all knowledge and wisdom, for though it has many omissions and contains much that is apocryphal, or at least wildly inaccurate, it scores over the older, more pedestrian work in two important respects.\n"
    "First, it is slightly cheaper; and secondly it has the words Don’t Panic inscribed in large friendly letters on its cover.\n"
    "But the story of this terrible, stupid Thursday, the story of its extraordi- nary consequences, and the story of how these consequences are inextricably intertwined with this remarkable book begins very simply.\n"
    "It begins with a house.\n";

/***********************************************************************************************************************
 * Prototypes
 */
void    myPDcallBack (void                  *pRefCon,
                      TRDP_APP_SESSION_T    appHandle,
                      const TRDP_PD_INFO_T  *pMsg,
                      UINT8                 *pData,
                      UINT32                dataSize);
void dbgOut (void           *pRefCon,
             TRDP_LOG_T     category,
             const CHAR8    *pTime,
             const CHAR8    *pFile,
             UINT16         LineNumber,
             const CHAR8    *pMsgStr);
void    usage (const char *appName);
void initPacketList (UINT32  pubBaseComId,
                     UINT32  subBaseComId);

void initPacketList (
    UINT32  pubBaseComId,
    UINT32  subBaseComId)
{
    unsigned int i;
    for (i = 0u; i < MAX_NO_OF_PKTS; i++)
    {
        gPubPackets[i].pubHandle    = 0;
        gPubPackets[i].subHandle    = 0;
        gPubPackets[i].comID        = pubBaseComId + i;
        gPubPackets[i].time         = PUBLISH_INTERVAL_BASE * i;
        gPubPackets[i].addr         = gDestIP;
        gPubPackets[i].dataSize     = DATASIZE_STEP * i;
        memcpy(gPubPackets[i].data, cDemoData, gPubPackets[i].dataSize);

        gSubPackets[i].pubHandle    = 0;
        gSubPackets[i].subHandle    = 0;
        gSubPackets[i].comID        = subBaseComId + i;
        gSubPackets[i].time         = SUBSCRIBE_TIMEOUT_BASE * i;
        gSubPackets[i].addr         = gOwnIP;
        gSubPackets[i].dataSize     = DATASIZE_STEP * i;
        memset(gSubPackets[i].data, 0, TRDP_MAX_PD_DATA_SIZE);
    }
}

/**********************************************************************************************************************/
/** callback routine for TRDP logging/error output
 *
 *  @param[in]      pRefCon         user supplied context pointer
 *  @param[in]      category        Log category (Error, Warning, Info etc.)
 *  @param[in]      pTime           pointer to NULL-terminated string of time stamp
 *  @param[in]      pFile           pointer to NULL-terminated string of source module
 *  @param[in]      LineNumber      line
 *  @param[in]      pMsgStr         pointer to NULL-terminated string
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

    printf("%s %s %16s:%-4d %s",
           strrchr(pTime, '-') + 1,
           catStr[category],
           (pF == NULL)? "" : pF + 1,
           LineNumber,
           pMsgStr);
}


/**********************************************************************************************************************/
/** callback routine for receiving TRDP traffic
 *
 *  @param[in]      pRefCon         user supplied context pointer
 *  @param[in]      pMsg            pointer to header/packet infos
 *  @param[in]      pData           pointer to data block
 *  @param[in]      dataSize        pointer to data size
 *  @retval         none
 */
void myPDcallBack (
    void                    *pRefCon,
    TRDP_APP_SESSION_T      appHandle,
    const TRDP_PD_INFO_T    *pMsg,
    UINT8                   *pData,
    UINT32                  dataSize)
{

    /*    Check why we have been called    */
    switch (pMsg->resultCode)
    {
        case TRDP_NO_ERR:
            printf("> ComID %d received, URef: %p\n", pMsg->comId, pMsg->pUserRef);
            break;

        case TRDP_TIMEOUT_ERR:
            /* The application can decide here if old data shall be invalidated or kept    */
            printf("> Packet timed out (ComID %d, SrcIP: %s)\n",
                   pMsg->comId,
                   vos_ipDotted(pMsg->srcIpAddr));
            break;
        default:
            printf("> Error on packet received (ComID %d), err = %d\n",
                   pMsg->comId,
                   pMsg->resultCode);
            break;
    }
}

/**********************************************************************************************************************/
/* Print a sensible usage message */
/**********************************************************************************************************************/
void usage (const char *appName)
{
    printf("Usage of %s\n", appName);
    printf("This tool sends PD messages to an ED and displays received PD packages.\n"
           "Arguments are:\n"
           "-o own IP address\n"
           "-t target IP address\n"
           "-c expecting base comID\n"
           "-s sending base comID\n"
           "-v print version and quit\n"
           );
}

/**********************************************************************************************************************/
/** main entry
 *
 *  @retval         0        no error
 *  @retval         1        some error
 */
int main (int argc, char * *argv)
{
    TRDP_APP_SESSION_T      appHandle;  /*    Our identifier to the library instance    */
    TRDP_ERR_T              err;
    TRDP_PD_CONFIG_T        pdConfiguration = {myPDcallBack, NULL, TRDP_PD_DEFAULT_SEND_PARAM, TRDP_FLAGS_CALLBACK,
                                               10000000, TRDP_TO_SET_TO_ZERO, 0};
    TRDP_MEM_CONFIG_T       dynamicConfig   = {NULL, RESERVED_MEMORY, {0}};
    TRDP_PROCESS_CONFIG_T   processConfig   = {"Me", "", 0, 0, TRDP_OPTION_BLOCK};
    int     rv = 0;
    unsigned int     ip[4];
    int     ch,i;
    UINT32  comId_In = SUBSCRIBE_COMID_BASE, comId_Out = PUBLISH_COMID_BASE;

    /****** Parsing the command line arguments */
    if (argc <= 1)
    {
        usage(argv[0]);
        return 1;
    }

    while ((ch = getopt(argc, argv, "t:o:c:s:h?v")) != -1)
    {
        switch (ch)
        {
            case 'c':
            {   /*  read comId    */
                if (sscanf(optarg, "%u",
                           &comId_In) < 1)
                {
                    usage(argv[0]);
                    exit(1);
                }
                break;
            }
            case 's':
            {   /*  read comId    */
                if (sscanf(optarg, "%u",
                           &comId_Out) < 1)
                {
                    usage(argv[0]);
                    exit(1);
                }
                break;
            }
            case 'o':
            {   /*  read ip    */
                if (sscanf(optarg, "%u.%u.%u.%u",
                           &ip[3], &ip[2], &ip[1], &ip[0]) < 4)
                {
                    usage(argv[0]);
                    exit(1);
                }
                gOwnIP = (ip[3] << 24u) | (ip[2] << 16u) | (ip[1] << 8u) | ip[0];
                break;
            }
            case 't':
            {   /*  read ip    */
                if (sscanf(optarg, "%u.%u.%u.%u",
                           &ip[3], &ip[2], &ip[1], &ip[0]) < 4)
                {
                    usage(argv[0]);
                    exit(1);
                }
                gDestIP = (ip[3] << 24) | (ip[2] << 16) | (ip[1] << 8) | ip[0];
                break;
            }
            case 'v':   /*  version */
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

    if (gDestIP == 0)
    {
        fprintf(stderr, "No destination address given!\n");
        usage(argv[0]);
        return 1;
    }

    /*    Init the library for callback operation    (PD only) */
    if (tlc_init(dbgOut,                           /* actually printf    */
                 NULL,
                 &dynamicConfig                    /* Use application supplied memory    */
                 ) != TRDP_NO_ERR)
    {
        printf("Initialization error\n");
        return 1;
    }

    /*    Open a session for callback operation    (PD only) */
    if (tlc_openSession(&appHandle,
                        gOwnIP,                     /* own IP address                     */
                        0,                          /* leader IP address                  */
                        NULL,                       /* no Marshalling                     */
                        &pdConfiguration, NULL,     /* system defaults for PD and MD      */
                        &processConfig) != TRDP_NO_ERR)
    {
        printf("Initialization error\n");
        return 1;
    }

    /*    Subscribe to PDs        */
    initPacketList(comId_Out, comId_In);

    for (i = 0; i < MAX_NO_OF_PKTS; i++)
    {
        err = tlp_subscribe(appHandle,                  /*    our application identifier           */
                            &gSubPackets[i].subHandle,  /*    our subscription identifier          */
                            (const void *) &i,           /*    user reference                       */
                            NULL,                       /*    callback function                    */
                            0u,
                            gSubPackets[i].comID,       /*    ComID                                */
                            0,                          /*    topocount: local consist only        */
                            0,
                            VOS_INADDR_ANY,             /*    Source to expect packets from        */
                            VOS_INADDR_ANY,
                            gSubPackets[i].addr,        /*    Default destination    (or MC Group) */
                            TRDP_FLAGS_CALLBACK,        /*    packet flags                         */
                            NULL,                       /*    default interface                    */
                            gSubPackets[i].time,        /*    Time out in us                       */
                            TRDP_TO_SET_TO_ZERO);       /*    delete invalid data on timeout       */

        if (err != TRDP_NO_ERR)
        {
            printf("prep pd receive error\n");
            tlc_terminate();
            return 1;
        }

        /*    Publish another PD        */

        err = tlp_publish(appHandle,                    /*    our application identifier    */
                          &gPubPackets[i].pubHandle,    /*    our pulication identifier     */
                          NULL, NULL,
                          0u,
                          gPubPackets[i].comID,         /*    ComID to send                 */
                          0,                            /*    local consist only            */
                          0,
                          0,                            /*    default source IP             */
                          gPubPackets[i].addr,          /*    where to send to              */
                          gPubPackets[i].time,          /*    Cycle time in ms              */
                          0,                            /*    not redundant                 */
                          TRDP_FLAGS_CALLBACK,          /*    Use callback for errors       */
                          NULL,                         /*    default qos and ttl           */
                          gPubPackets[i].dataSize ? gPubPackets[i].data : NULL,      /*    initial data */
                          gPubPackets[i].dataSize       /*    data size                     */
                          );                            /*    no ladder                     */


        if (err != TRDP_NO_ERR)
        {
            printf("prep pd publish error\n");
            tlc_terminate();
            return 1;
        }
    }


    /*
     Enter the main processing loop.
     */
    while (1)
    {
        fd_set  rfds;
        INT32   noOfDesc;
        struct timeval  tv;
        struct timeval  max_tv = {0, 100000};

        /*
         Prepare the file descriptor set for the select call.
         Additional descriptors can be added here.
         */
        FD_ZERO(&rfds);
        /*
         Compute the min. timeout value for select and return descriptors to wait for.
         This way we can guarantee that PDs are sent in time...
         */
        (void) tlc_getInterval(appHandle,
                              (TRDP_TIME_T *) &tv,
                              (TRDP_FDS_T *) &rfds,
                              &noOfDesc);

        /*
         The wait time for select must consider cycle times and timeouts of
         the PD packets received or sent.
         If we need to poll something faster than the lowest PD cycle,
         we need to set the maximum timeout ourselfs
         */

        if (vos_cmpTime((TRDP_TIME_T *) &tv, (TRDP_TIME_T *) &max_tv) > 0)
        {
            tv = max_tv;
        }

        /*
         Select() will wait for ready descriptors or timeout,
         what ever comes first.
         */

        rv = vos_select((int)noOfDesc + 1, &rfds, NULL, NULL, &tv);

        /* printf("Pending events: %d\n", rv); */
        /*
         Check for overdue PDs (sending and receiving)
         Send any PDs if it's time...
         Detect missing PDs...
         'rv' will be updated to show the handled events, if there are
         more than one...
         The callback function will be called from within the trdp_work
         function (in it's context and thread)!
         */

        err = tlc_process(appHandle, (TRDP_FDS_T *) &rfds, &rv);

        /*
         Handle other ready descriptors...
         */
        if (rv > 0)
        {
            printf("other descriptors were ready\n");
        }
        else
        {
            /* printf("looping...\n"); */
        }

        /* Update the information, that is sent */
        /*
           sprintf((char *)gBuffer, "Ping for the %dth. time.", hugeCounter++);
           err = tlp_put(appHandle, pubHandle, (const UINT8 *) gBuffer, GBUFFER_SIZE);
         */
        if (err != TRDP_NO_ERR)
        {
            printf("tlc_process error\n");
            rv = 1;
        }

        /* Display received information */
        /*
           if (gInputBuffer[0] > 0) / * FIXME Better solution would be: global flag, that is set in the callback
           function to
                                    indicate new data * /
           {
              printf("# %s ", gInputBuffer);
              memset(gInputBuffer, 0, sizeof(gInputBuffer));
           }
         */
    }   /*    Bottom of while-loop    */

    /*
     *    We always clean up behind us!
     */
    for (i = 0; i < MAX_NO_OF_PKTS; i++)
    {
        tlp_unpublish(appHandle, gPubPackets[i].pubHandle);
        tlp_unsubscribe(appHandle, gSubPackets[i].subHandle);
    }
    tlc_closeSession(appHandle);
    tlc_terminate();

    return rv;
}
