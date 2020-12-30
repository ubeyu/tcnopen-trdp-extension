/**********************************************************************************************************************/
/**
 * @file            tau_tti.c
 *
 * @brief           Functions for train topology information access
 *
 * @details         The TTI subsystem maintains a pointer to the TAU_TTDB struct in the TRDP session struct.
 *                  That TAU_TTDB struct keeps the subscription and listener handles, the current TTDB directories and
 *                  a pointer list to consist infos (in network format). On init, most TTDB data is requested from the
 *                  ECSP plus the own consist info.
 *                  This data is automatically updated if an inauguration is detected. Additional consist infos are
 *                  requested on demand, only.
 *                  Because of the asynchronous behavior of the TTI subsystem, most functions in tau_tti.c may return
 *                  TRDP_NODATA_ERR on first invocation.
 *                  They should be called again after 1...3 seconds (3s is the timeout for most MD replies).
 *
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          B. Loehr (initial version)
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright Bombardier Transportation Inc. or its subsidiaries and others, 2016-2020. All rights reserved.
 */
/*
* $Id: tau_tti.c 2174 2020-07-10 13:45:53Z bloehr $
*
*      BL 2020-07-10: Ticket #292 tau_getTrnVehCnt( ) not working if OpTrnDir is not already valid
*      BL 2020-07-09: Ticket #298 Create consist info entry error -> check for false data and empty arrays
*      BL 2020-07-08: Ticket #297 Store Operation Train Dir error
*      BL 2019-12-06: Ticket #299 ComId 100 shall be subscribed from two sources but only one needs to be received
*      SB 2019-08-13: Ticket #268 Handling Redundancy Switchover of DNS/ECSP server
*      BL 2019-06-17: Ticket #264 Provide service oriented interface
*      BL 2019-06-17: Ticket #162 Independent handling of PD and MD to reduce jitter
*      BL 2019-06-17: Ticket #161 Increase performance
*      BL 2019-06-17: Ticket #191 Add provisions for TSN / Hard Real Time (open source)
*      V 2.0.0 --------- ^^^ -----------
*      V 1.4.2 --------- vvv -----------
*      BL 2019-06-11: Ticket #253 Incorrect storing of TTDB_STATIC_CONSIST_INFO_REPLY from network packet into local copy
*      BL 2019-05-15: Ticket #254 API of TTI to get OwnOpCstNo and OwnTrnCstNo
*      BL 2019-05-15: Ticket #255 opTrnState of pTTDB isn't copied completely
*      BL 2019-05-15: Ticket #257 TTI: register for ComID100 PD to both valid multicast addresses
*      BL 2019-05-15: Ticket #245 Incorrect storing of TTDB_OP_TRAIN_DIRECTORY_INFO from network packet into local copy
*      SB 2019-02-06: Added OpTrn topocnt changed log message (PD100), wait in mdCallback only when topocnt changed
*      SB 2019-01-31: fixed reference of waitForInaug semaphore pointer in ttiMDCallback
*      BL 2019-01-24: ttiStoreOpTrnDir returns changed flag
*      BL 2019-01-24: Missing PD100 -> WARNING (not ERROR) log
*      BL 2018-08-07: Ticket #183 tau_getOwnIds declared but not defined
*      BL 2018-06-20: Ticket #184: Building with VS 2015: WIN64 and Windows threads (SOCKET instead of INT32)
*      BL 2017-11-28: Ticket #180 Filtering rules for DestinationURI does not follow the standard
*      BL 2017-11-13: Ticket #176 TRDP_LABEL_T breaks field alignment -> TRDP_NET_LABEL_T
*     AHW 2017-11-08: Ticket #179 Max. number of retries (part of sendParam) of a MD request needs to be checked
*      BL 2017-05-08: Compiler warnings, doxygen comment errors
*      BL 2017-04-28: Ticket #155: Kill trdp_proto.h - move definitions to iec61375-2-3.h
*      BL 2017-03-13: Ticket #154 ComIds and DSIds literals (#define TRDP_...) in trdp_proto.h too long
*      BL 2017-02-10: Ticket #129 Found a bug which yields wrong output params and potentially segfaults
*      BL 2017-02-08: Ticket #142 Compiler warnings / MISRA-C 2012 issues
*      BL 2016-02-18: Ticket #7: Add train topology information support
*/

/***********************************************************************************************************************
 * INCLUDES
 */

#include <string.h>
#include <stdio.h>

#include "trdp_if_light.h"
#include "trdp_utils.h"
#include "tau_marshall.h"
#include "tau_tti.h"
#include "vos_sock.h"
#include "tau_dnr.h"

#include "tau_cstinfo.c"

/***********************************************************************************************************************
 * DEFINES
 */

#define TTI_CACHED_CONSISTS  8u             /**< We hold this number of consist infos (ca. 105kB) */

/***********************************************************************************************************************
 * TYPEDEFS
 */

typedef struct TAU_TTDB
{
    TRDP_SUB_T                      pd100SubHandle1;
    TRDP_SUB_T                      pd100SubHandle2;
    TRDP_LIS_T                      md101Listener1;
    TRDP_LIS_T                      md101Listener2;
    TRDP_OP_TRAIN_DIR_STATUS_INFO_T opTrnState;
    TRDP_OP_TRAIN_DIR_T             opTrnDir;
    TRDP_TRAIN_DIR_T                trnDir;
    TRDP_TRAIN_NET_DIR_T            trnNetDir;
    UINT32                          noOfCachedCst;
    UINT32                          cstSize[TRDP_MAX_CST_CNT];
    TRDP_CONSIST_INFO_T             *cstInfo[TRDP_MAX_CST_CNT];     /**< NOTE: the consist info is a variable sized
                                                            struct / array and is stored in network representation */
} TAU_TTDB_T;

/***********************************************************************************************************************
 *   Locals
 */

static void ttiRequestTTDBdata (TRDP_APP_SESSION_T  appHandle,
                                UINT32              comID,
                                const TRDP_UUID_T   cstUUID);

static void ttiGetUUIDfromLabel (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_UUID_T         cstUUID,
    const TRDP_LABEL_T  cstLabel);

/**********************************************************************************************************************/
/**    Function returns the UUID for the given UIC ID
 *      We need to search in the OP_TRAIN_DIR the OP_VEHICLE where the vehicle is the
 *      first one in the consist and its name matches.
 *      Note: The first vehicle in a consist has the same ID as the consist it is belonging to (5.3.3.2.5)
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[in]      cstUUID         Consist UUID.
 *  @param[in]      cstLabel        Consist label.
 *
 *  @retval         none
 *
 */
static void ttiGetUUIDfromLabel (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_UUID_T         cstUUID,
    const TRDP_LABEL_T  cstLabel)
{
    UINT32  i;
    UINT32  j;

    /* Search the vehicles in the OP_TRAIN_DIR for a matching vehID */

    for (i = 0u; i < appHandle->pTTDB->opTrnDir.opVehCnt; i++)
    {
        if (vos_strnicmp(appHandle->pTTDB->opTrnDir.opVehList[i].vehId, cstLabel, sizeof(TRDP_NET_LABEL_T)) == 0)
        {
            /* vehicle found, is it the first in the consist?   */
            UINT8 opCstNo = appHandle->pTTDB->opTrnDir.opVehList[i].ownOpCstNo;
            for (j = 0u; j < appHandle->pTTDB->opTrnDir.opCstCnt; j++)
            {
                if (opCstNo == appHandle->pTTDB->opTrnDir.opCstList[j].opCstNo)
                {
                    memcpy(cstUUID, appHandle->pTTDB->opTrnDir.opCstList[j].cstUUID, sizeof(TRDP_UUID_T));
                    return;
                }
            }
        }
    }
    /* not found    */
    memset(cstUUID, 0, sizeof(TRDP_UUID_T));
}

/**********************************************************************************************************************/
/** Check own consist info
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[in]      pTelegram       Pointer to the consist info.
 *
 *  @retval         != 0            if pTelegram is own consist
 *
 */
static BOOL8   ttiIsOwnCstInfo (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_CONSIST_INFO_T *pTelegram)
{
    UINT32 i;
    for (i = 0u; i < appHandle->pTTDB->opTrnDir.opCstCnt; i++)
    {
        if (appHandle->pTTDB->opTrnState.ownTrnCstNo == appHandle->pTTDB->opTrnDir.opCstList[i].trnCstNo)
        {
            return memcmp(appHandle->pTTDB->opTrnDir.opCstList[i].cstUUID, pTelegram->cstUUID,
                          sizeof(TRDP_UUID_T)) == 0u;
        }
    }
    return FALSE;
}

/**********************************************************************************************************************/
/**    Function called on reception of process data
 *
 *  Handle and process incoming data, update our data store
 *
 *  @param[in]      pRefCon         unused.
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[in]      pMsg            Pointer to the message info (header etc.)
 *  @param[in]      pData           Pointer to the network buffer.
 *  @param[in]      dataSize        Size of the received data
 *
 *  @retval         none
 *
 */
static void ttiPDCallback (
    void                    *pRefCon,
    TRDP_APP_SESSION_T      appHandle,
    const TRDP_PD_INFO_T    *pMsg,
    UINT8                   *pData,
    UINT32                  dataSize)
{
    int         changed         = 0;
    VOS_SEMA_T  waitForInaug    = (VOS_SEMA_T) pMsg->pUserRef;
    static      TRDP_IP_ADDR_T  sDestMC = VOS_INADDR_ANY;

    pRefCon = pRefCon;

    if (pMsg->comId == TTDB_STATUS_COMID)
    {
        if ((pMsg->resultCode == TRDP_NO_ERR) &&
            (dataSize <= sizeof(TRDP_OP_TRAIN_DIR_STATUS_INFO_T)))
        {
            TRDP_OP_TRAIN_DIR_STATUS_INFO_T *pTelegram = (TRDP_OP_TRAIN_DIR_STATUS_INFO_T *) pData;
            UINT32 crc;
            TAU_DNR_ENTRY_T  *pDNRIp   = (TAU_DNR_ENTRY_T *) appHandle->pUser;

            /* check the crc:   */
            crc = vos_sc32(0xFFFFFFFFu, (const UINT8 *) &pTelegram->state, sizeof(TRDP_OP_TRAIN_DIR_STATE_T) - 4);
            if (crc != vos_ntohl(pTelegram->state.crc))
            {
                vos_printLog(VOS_LOG_WARNING, "CRC error of received operational status info (%08x != %08x)!\n",
                             crc, vos_ntohl(pTelegram->state.crc))
                    (void) tlc_setOpTrainTopoCount(appHandle, 0);
                return;
            }

            /* This is an addition purely done for TRDP to handle DNS/ECSP Redundancy switchover.
               PD 100 is always sent from the original IP address of switch and not the virtual one.
               Everytime a PD 100 is received, we store its source IP address in the appHandle->pUser.
               This will change the (server) IP to which the DNS requests will be sent. */

            if ((pDNRIp != NULL) && (pMsg->srcIpAddr != VOS_INADDR_ANY))
            {
                pDNRIp->ipAddr = pMsg->srcIpAddr;
            }

            /* Store the state locally */
            memcpy(
                &appHandle->pTTDB->opTrnState,
                pTelegram,
                (sizeof(TRDP_OP_TRAIN_DIR_STATUS_INFO_T) <
                 dataSize) ? sizeof(TRDP_OP_TRAIN_DIR_STATUS_INFO_T) : dataSize);

            /* unmarshall manually:   */
            appHandle->pTTDB->opTrnState.etbTopoCnt         = vos_ntohl(pTelegram->etbTopoCnt);
            appHandle->pTTDB->opTrnState.state.opTrnTopoCnt = vos_ntohl(pTelegram->state.opTrnTopoCnt);
            appHandle->pTTDB->opTrnState.state.crc = vos_ntohl(pTelegram->state.crc);

            /* vos_printLog(VOS_LOG_INFO, "---> Operational status info received on %p\n", appHandle); */

            /* Has the etbTopoCnt changed? */
            if (appHandle->etbTopoCnt != appHandle->pTTDB->opTrnState.etbTopoCnt)
            {
                vos_printLog(VOS_LOG_INFO, "ETB topocount changed (old: 0x%08x, new: 0x%08x) on %p!\n",
                             appHandle->etbTopoCnt, appHandle->pTTDB->opTrnState.etbTopoCnt, (void *) appHandle);
                changed++;
                (void) tlc_setETBTopoCount(appHandle, appHandle->pTTDB->opTrnState.etbTopoCnt);
            }

            if (appHandle->opTrnTopoCnt != appHandle->pTTDB->opTrnState.state.opTrnTopoCnt)
            {
                vos_printLog(VOS_LOG_INFO,
                             "OpTrn topocount changed (old: 0x%08x, new: 0x%08x) on %p!\n",
                             appHandle->opTrnTopoCnt,
                             appHandle->pTTDB->opTrnState.state.opTrnTopoCnt,
                             (void *) appHandle);
                changed++;
                (void) tlc_setOpTrainTopoCount(appHandle, appHandle->pTTDB->opTrnState.state.opTrnTopoCnt);
            }
            /* remember the received telegram's destination (MC)    */
            sDestMC = pMsg->destIpAddr;
        }
        else if (pMsg->resultCode == TRDP_TIMEOUT_ERR )
        {
            /* clear the topocounts only if the timeout came from the active subscription */

            if ((sDestMC == VOS_INADDR_ANY) || (sDestMC == pMsg->destIpAddr))
            {
                vos_printLog(VOS_LOG_WARNING, "---> Operational status info timed out! Invalidating topocounts on %p!\n",
                             (void *)appHandle);

                if (appHandle->etbTopoCnt != 0u)
                {
                    changed++;
                    (void) tlc_setETBTopoCount(appHandle, 0u);
                }
                if (appHandle->opTrnTopoCnt != 0u)
                {
                    changed++;
                    (void) tlc_setOpTrainTopoCount(appHandle, 0u);
                }
            }
        }
        else
        {
            vos_printLog(VOS_LOG_INFO, "---> Unsolicited msg received on %p!\n",
                         (void *)appHandle);
        }
        if ((changed > 0) && (waitForInaug != NULL))
        {
            vos_semaGive(waitForInaug);
        }
    }
}

/**********************************************************************************************************************/
/*  Functions to convert TTDB network packets into local (static) representation                                      */
/**********************************************************************************************************************/

/**********************************************************************************************************************/
/** Store the operational train directory
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[in]      pData           Pointer to the network buffer.
 *
 *  @retval         != 0            if topoCnt changed
 *
 */
static BOOL8 ttiStoreOpTrnDir (
    TRDP_APP_SESSION_T  appHandle,
    UINT8               *pData)
{
    TRDP_OP_TRAIN_DIR_T *pTelegram = (TRDP_OP_TRAIN_DIR_T *) pData;
    UINT32  size;
    BOOL8   changedTopoCnt;

    /* we have to unpack the data, copy up to OP_CONSIST */
    if (pTelegram->opCstCnt > TRDP_MAX_CST_CNT)
    {
        vos_printLog(VOS_LOG_WARNING, "Max count of consists of received operational dir exceeded (%d)!\n",
                     pTelegram->opCstCnt);
        return 0;
    }

    /* 8 Bytes up to opCstCnt plus number of Consists  */
    size = 8 + pTelegram->opCstCnt * sizeof(TRDP_OP_CONSIST_T);
    memcpy(&appHandle->pTTDB->opTrnDir, pData, size);
    pData += size + 3;              /* jump to cnt  */
    appHandle->pTTDB->opTrnDir.opVehCnt = *pData++;
    size = appHandle->pTTDB->opTrnDir.opVehCnt * sizeof(TRDP_OP_VEHICLE_T);     /* copy array only! (#297)    */
    memcpy(appHandle->pTTDB->opTrnDir.opVehList, pData, size);

    /* unmarshall manually and update the opTrnTopoCount   */

    appHandle->pTTDB->opTrnDir.opTrnTopoCnt = *(UINT32 *) (pData + size);

    appHandle->pTTDB->opTrnDir.opTrnTopoCnt = vos_ntohl(appHandle->pTTDB->opTrnDir.opTrnTopoCnt);

    changedTopoCnt = (tlc_getOpTrainTopoCount(appHandle) != appHandle->pTTDB->opTrnDir.opTrnTopoCnt);
    (void) tlc_setOpTrainTopoCount(appHandle, appHandle->pTTDB->opTrnDir.opTrnTopoCnt);
    return changedTopoCnt;
}

/**********************************************************************************************************************/
/** Store the train directory
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[in]      pData           Pointer to the network buffer.
 *
 *  @retval         none
 *
 */
static void ttiStoreTrnDir (
    TRDP_APP_SESSION_T  appHandle,
    UINT8               *pData)
{
    TRDP_TRAIN_DIR_T *pTelegram = (TRDP_TRAIN_DIR_T *) pData;
    UINT32 size, i;

    /* we have to unpack the data, copy up to CONSIST */
    if (pTelegram->cstCnt > TRDP_MAX_CST_CNT)
    {
        vos_printLog(VOS_LOG_WARNING, "Max count of consists of received train dir exceeded (%d)!\n",
                     pTelegram->cstCnt);
        return;
    }

    /* 4 Bytes up to cstCnt plus number of Consists  */
    size = 4 + pTelegram->cstCnt * sizeof(TRDP_CONSIST_T);
    memcpy(&appHandle->pTTDB->trnDir, pData, size);
    pData += size;              /* jump to trnTopoCount  */

    /* unmarshall manually and update the trnTopoCount   */
    appHandle->pTTDB->trnDir.trnTopoCnt = vos_ntohl((*(UINT32 *)pData));   /* copy trnTopoCnt as well    */

    /* swap the consist topoCnts    */
    for (i = 0; i < appHandle->pTTDB->trnDir.cstCnt; i++)
    {
        appHandle->pTTDB->trnDir.cstList[i].cstTopoCnt = vos_ntohl(appHandle->pTTDB->trnDir.cstList[i].cstTopoCnt);
    }
}

/**********************************************************************************************************************/
/** Store the train network directory
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[in]      pData           Pointer to the network buffer.
 *
 *  @retval         none
 *
 */
static void ttiStoreTrnNetDir (
    TRDP_APP_SESSION_T  appHandle,
    UINT8               *pData)
{
    TRDP_TRAIN_NET_DIR_T *pTelegram = (TRDP_TRAIN_NET_DIR_T *) pData;
    UINT32 size, i;

    /* we have to unpack the data, copy up to CONSIST */

    appHandle->pTTDB->trnNetDir.reserved01  = 0;
    appHandle->pTTDB->trnNetDir.entryCnt    = vos_ntohs(pTelegram->entryCnt);
    if (appHandle->pTTDB->trnNetDir.entryCnt > TRDP_MAX_CST_CNT)
    {
        vos_printLog(VOS_LOG_WARNING, "Max count of consists of received train net dir exceeded (%d)!\n",
                     vos_ntohs(appHandle->pTTDB->trnNetDir.entryCnt));
        return;
    }

    /* 4 Bytes up to cstCnt plus number of Consists  */
    size = appHandle->pTTDB->trnNetDir.entryCnt * sizeof(TRDP_TRAIN_NET_DIR_ENTRY_T);
    memcpy(&appHandle->pTTDB->trnNetDir.trnNetDir[0], pData, size);
    pData += 4 + size;              /* jump to etbTopoCnt  */

    /* unmarshall manually and update the etbTopoCount   */
    appHandle->pTTDB->trnNetDir.etbTopoCnt = vos_ntohl((*(UINT32 *)pData));   /* copy etbTopoCnt as well    */

    /* swap the consist network properties    */
    for (i = 0; i < appHandle->pTTDB->trnNetDir.entryCnt; i++)
    {
        appHandle->pTTDB->trnNetDir.trnNetDir[i].cstNetProp = vos_ntohl(
                appHandle->pTTDB->trnNetDir.trnNetDir[i].cstNetProp);
    }
}

/**********************************************************************************************************************/
/** Clear Consist Info Entry
 *
 *  Remove traces of old consist info
 *
 *  @param[in]      pData           Pointer to the consist info.
 *
 *  @retval         none
 *
 */
static void ttiFreeCstInfoEntry (
    TRDP_CONSIST_INFO_T *pData)
{
    if (pData->pVehInfoList != NULL)
    {
        vos_memFree(pData->pVehInfoList);
    }
    if (pData->pEtbInfoList != NULL)
    {
        vos_memFree(pData->pEtbInfoList);
    }
    if (pData->pFctInfoList != NULL)
    {
        vos_memFree(pData->pFctInfoList);
    }
    if (pData->pCltrCstInfoList != NULL)
    {
        vos_memFree(pData->pCltrCstInfoList);
    }
}

/**********************************************************************************************************************/
/** Create new consist info entry
 *
 *  Create new consist info entry from received telegram
 *
 *  @param[out]     pDest           Pointer to the struct to be received.
 *  @param[in]      pData           Pointer to the network buffer.
 *  @param[in]      dataSize        Size of the received data
 *
 *  @retval         TRDP_NO_ERR         no error
 *  @retval         TRDP_PACKET_ERR     packet malformed
 *  @retval         TRDP_MEM_ERR        out of memory
 *
 */
static TRDP_ERR_T ttiCreateCstInfoEntry (
    TRDP_CONSIST_INFO_T *pDest,
    UINT8               *pData,
    UINT32              dataSize)
{
    UINT32  idx;
    UINT8   *pEnd = pData + dataSize;

    /** Exit if the packet is too small.
      (Actually this should be checked more often to prevent DoS or stack/memory attacks)
     */
    if (dataSize < sizeof(TRDP_CONSIST_INFO_T))    /**< minimal size of a consist info telegram */
    {
        return TRDP_PACKET_ERR;
    }

    pDest->version.ver  = *pData++;
    pDest->version.rel  = *pData++;
    pDest->cstClass     = *pData++;
    pDest->reserved01   = *pData++;
    memcpy(pDest->cstId, pData, TRDP_MAX_LABEL_LEN);
    pData += TRDP_MAX_LABEL_LEN;
    memcpy(pDest->cstType, pData, TRDP_MAX_LABEL_LEN);
    pData += TRDP_MAX_LABEL_LEN;
    memcpy(pDest->cstOwner, pData, TRDP_MAX_LABEL_LEN);
    pData += TRDP_MAX_LABEL_LEN;
    memcpy(pDest->cstUUID, pData, sizeof(TRDP_UUID_T));
    pData += sizeof(TRDP_UUID_T);
    pDest->reserved02 = vos_ntohl(*(UINT32 *)pData);
    pData += sizeof(UINT32);
    pDest->cstProp.ver.ver  = *pData++;
    pDest->cstProp.ver.rel  = *pData++;
    pDest->cstProp.len      = vos_ntohs(*(UINT16 *)pData);
    pData += sizeof(UINT16);
    pDest->cstProp.prop[0] = 0;         /* Note: properties are not supported */
    pData += pDest->cstProp.len;        /* we need to account for them anyway */
    pDest->cstProp.len  = 0;
    pDest->reserved03   = vos_ntohs(*(UINT16 *)pData);
    pData += sizeof(UINT16);

    if (pData > pEnd)
    {
        return TRDP_PACKET_ERR;
    }

    /* Dynamic sized ETB info */
    pDest->etbCnt   = vos_ntohs(*(UINT16 *)pData);
    pData           += sizeof(UINT16);

    pDest->pEtbInfoList = (TRDP_ETB_INFO_T *) vos_memAlloc(sizeof(TRDP_ETB_INFO_T) * pDest->etbCnt);
    if (pDest->pEtbInfoList == NULL)
    {
        pDest->etbCnt = 0;
        return TRDP_MEM_ERR;
    }
    for (idx = 0u; idx < pDest->etbCnt; idx++)
    {
        pDest->pEtbInfoList->etbId      = *pData++;
        pDest->pEtbInfoList->cnCnt      = *pData++;
        pDest->pEtbInfoList->reserved01 = vos_ntohs(*(UINT16 *)pData);
        pData += sizeof(UINT16);
    }
    /* pData += sizeof(TRDP_ETB_INFO_T) * pDest->etbCnt; */ /* Incremented while copying */

    if (pData > pEnd)
    {
        return TRDP_PACKET_ERR;
    }

    pDest->reserved04 = vos_ntohs(*(UINT16 *)pData);
    pData += sizeof(UINT16);

    /* Dynamic sized Vehicle info */
    pDest->vehCnt   = vos_ntohs(*(UINT16 *)pData);
    pData           += sizeof(UINT16);

    pDest->pVehInfoList = (TRDP_VEHICLE_INFO_T *) vos_memAlloc(sizeof(TRDP_VEHICLE_INFO_T) * pDest->vehCnt);
    if (pDest->pVehInfoList == NULL)
    {
        pDest->vehCnt   = 0;
        pDest->etbCnt   = 0;
        vos_memFree(pDest->pEtbInfoList);
        pDest->pEtbInfoList = NULL;
        return TRDP_MEM_ERR;
    }
    /* copy the vehicle list */
    for (idx = 0u; idx < pDest->vehCnt; idx++)
    {
        memcpy(pDest->pVehInfoList->vehId, pData, sizeof(TRDP_NET_LABEL_T));
        pData += sizeof(TRDP_NET_LABEL_T);
        memcpy(pDest->pVehInfoList->vehType, pData, sizeof(TRDP_NET_LABEL_T));
        pData += sizeof(TRDP_NET_LABEL_T);
        pDest->pVehInfoList->vehOrient          = *pData++;
        pDest->pVehInfoList->cstVehNo           = *pData++;
        pDest->pVehInfoList->tractVeh           = *pData++;
        pDest->pVehInfoList->reserved01         = *pData++;
        pDest->pVehInfoList->vehProp.ver.ver    = *pData++;
        pDest->pVehInfoList->vehProp.ver.rel    = *pData++;
        pDest->pVehInfoList->vehProp.len        = vos_ntohs(*(UINT16 *)pData);
        pData += sizeof(UINT16);
        if (pDest->pVehInfoList->vehProp.len != 0u)
        {
            memcpy(pDest->pVehInfoList->vehProp.prop, pData, pDest->pVehInfoList->vehProp.len);
            pData += pDest->pVehInfoList->vehProp.len;
        }
    }

    /* pData += sizeof(TRDP_VEHICLE_INFO_T) * pDest->vehCnt; */ /* Incremented while copying */

    pDest->reserved05 = vos_ntohs(*(UINT16 *)pData);
    pData += sizeof(UINT16);

    /* Dynamically sized Function info */

    pDest->fctCnt   = vos_ntohs(*(UINT16 *)pData);
    pData           += sizeof(UINT16);

    if (pDest->fctCnt > 0)
    {
        pDest->pFctInfoList = (TRDP_FUNCTION_INFO_T *) vos_memAlloc(sizeof(TRDP_FUNCTION_INFO_T) * pDest->fctCnt);
        if (pDest->pFctInfoList == NULL)
        {
            pDest->fctCnt   = 0;
            pDest->etbCnt   = 0;
            vos_memFree(pDest->pEtbInfoList);
            pDest->pEtbInfoList = NULL;
            pDest->vehCnt       = 0;
            vos_memFree(pDest->pVehInfoList);
            pDest->pVehInfoList = NULL;
            return TRDP_MEM_ERR;
        }

        for (idx = 0u; idx < pDest->fctCnt; idx++)
        {
            memcpy(pDest->pFctInfoList->fctName, pData, sizeof(TRDP_NET_LABEL_T));
            pData += sizeof(TRDP_NET_LABEL_T);
            pDest->pFctInfoList->fctId = vos_ntohs(*(UINT16 *)pData);
            pData += sizeof(UINT16);
            pDest->pFctInfoList->grp        = *pData++;
            pDest->pFctInfoList->reserved01 = *pData++;
            pDest->pFctInfoList->cstVehNo   = *pData++;
            pDest->pFctInfoList->etbId      = *pData++;
            pDest->pFctInfoList->cnId       = *pData++;
            pDest->pFctInfoList->reserved02 = *pData++;
        }
        /* pData += sizeof(TRDP_FUNCTION_INFO_T) * pDest->fctCnt; */ /* Incremented while copying */
    }

    pDest->reserved06 = vos_ntohs(*(UINT16 *)pData);
    pData += sizeof(UINT16);

    /* Dynamically sized Closed Train Consist Composition info */

    pDest->cltrCstCnt = vos_ntohs(*(UINT16 *)pData);
    pData += sizeof(UINT16);

    if (pDest->cltrCstCnt > 0)
    {
        pDest->pCltrCstInfoList = (TRDP_CLTR_CST_INFO_T *) vos_memAlloc(sizeof(TRDP_CLTR_CST_INFO_T) * pDest->cltrCstCnt);
        if (pDest->pCltrCstInfoList == NULL)
        {
            pDest->cltrCstCnt   = 0;
            pDest->etbCnt       = 0;
            vos_memFree(pDest->pEtbInfoList);
            pDest->pEtbInfoList = NULL;
            pDest->vehCnt       = 0;
            vos_memFree(pDest->pVehInfoList);
            pDest->pVehInfoList = NULL;
            pDest->fctCnt       = 0;
            vos_memFree(pDest->pFctInfoList);
            pDest->pFctInfoList = NULL;
            return TRDP_MEM_ERR;
        }

        for (idx = 0u; idx < pDest->cltrCstCnt; idx++)
        {
            memcpy(pDest->pCltrCstInfoList->cltrCstUUID, pData, sizeof(TRDP_UUID_T));
            pData += sizeof(TRDP_UUID_T);
            pDest->pCltrCstInfoList->cltrCstOrient  = *pData++;
            pDest->pCltrCstInfoList->cltrCstNo      = *pData++;
            pDest->pCltrCstInfoList->reserved01     = vos_ntohs(*(UINT16 *)pData);
            pData += sizeof(UINT16);
        }

        /* pData += sizeof(TRDP_CLTR_CST_INFO_T) * pDest->cltrCstCnt; */ /* Incremented while copying */
    }
    pDest->cstTopoCnt = vos_ntohl(*(UINT32 *)pData);
    return TRDP_NO_ERR;
}

/**********************************************************************************************************************/
/** Store the received consist info
 *
 *  Find an appropriate location to store the received consist info
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[in]      pData           Pointer to the network buffer.
 *  @param[in]      dataSize        Size of the received data
 *
 *  @retval         none
 *
 */
static void ttiStoreCstInfo (
    TRDP_APP_SESSION_T  appHandle,
    UINT8               *pData,
    UINT32              dataSize)
{
    TRDP_CONSIST_INFO_T *pTelegram = (TRDP_CONSIST_INFO_T *) pData;
    UINT32 curEntry = 0;

    if (ttiIsOwnCstInfo(appHandle, pTelegram) == TRUE)
    {
        if (appHandle->pTTDB->cstInfo[curEntry] != NULL)
        {
            ttiFreeCstInfoEntry(appHandle->pTTDB->cstInfo[curEntry]);
            vos_memFree(appHandle->pTTDB->cstInfo[curEntry]);
            appHandle->pTTDB->cstInfo[curEntry] = NULL;
        }
    }
    else
    {
        UINT32 l_index;
        curEntry = 1;
        /* check if already loaded */
        for (l_index = 1; l_index < TRDP_MAX_CST_CNT; l_index++)
        {
            if (appHandle->pTTDB->cstInfo[l_index] != NULL &&
                appHandle->pTTDB->cstInfo[l_index]->cstTopoCnt != 0 &&
                memcmp(appHandle->pTTDB->cstInfo[l_index]->cstUUID, pTelegram->cstUUID, sizeof(TRDP_UUID_T)) == 0)
            {
                ttiFreeCstInfoEntry(appHandle->pTTDB->cstInfo[l_index]);
                vos_memFree(appHandle->pTTDB->cstInfo[l_index]);
                appHandle->pTTDB->cstInfo[l_index] = NULL;
                curEntry = l_index;
                break;
            }
        }
    }

    /* Allocate space for the consist info */
    appHandle->pTTDB->cstInfo[curEntry] = (TRDP_CONSIST_INFO_T *) vos_memAlloc(sizeof(TRDP_CONSIST_INFO_T));
    if (appHandle->pTTDB->cstInfo[curEntry] == NULL)
    {
        vos_printLogStr(VOS_LOG_ERROR, "Consist info could not be stored!");
        return;
    }

    /* We do convert and allocate more memory for the several parts of the consist info inside. */

    if (ttiCreateCstInfoEntry(appHandle->pTTDB->cstInfo[curEntry], pData, dataSize) != TRDP_NO_ERR)
    {
        vos_memFree(appHandle->pTTDB->cstInfo[curEntry]);
        appHandle->pTTDB->cstSize[curEntry] = 0u;
        vos_printLogStr(VOS_LOG_ERROR, "Parts of consist info could not be stored!");
        return;
    }
    appHandle->pTTDB->cstSize[curEntry] = sizeof(TRDP_CONSIST_INFO_T);
}

/**********************************************************************************************************************/
/**    Function called on reception of message data
 *
 *  Handle and process incoming data, update our data store
 *
 *  @param[in]      pRefCon         unused.
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[in]      pMsg            Pointer to the message info (header etc.)
 *  @param[in]      pData           Pointer to the network buffer.
 *  @param[in]      dataSize        Size of the received data
 *
 *  @retval         none
 *
 */
static void ttiMDCallback (
    void                    *pRefCon,
    TRDP_APP_SESSION_T      appHandle,
    const TRDP_MD_INFO_T    *pMsg,
    UINT8                   *pData,
    UINT32                  dataSize)
{
    VOS_SEMA_T waitForInaug = (VOS_SEMA_T) pMsg->pUserRef;

    (void) pRefCon;

    if (pMsg->resultCode == TRDP_NO_ERR)
    {
        if ((pMsg->comId == TTDB_OP_DIR_INFO_COMID) ||      /* TTDB notification */
            (pMsg->comId == TTDB_OP_DIR_INFO_REP_COMID))
        {
            if (dataSize <= sizeof(TRDP_OP_TRAIN_DIR_T))
            {
                if (ttiStoreOpTrnDir(appHandle, pData))
                {
                    if (waitForInaug != NULL)
                    {
                        vos_semaGive(waitForInaug);           /* Signal new inauguration    */
                    }
                }
            }
        }
        else if (pMsg->comId == TTDB_TRN_DIR_REP_COMID)
        {
            if (dataSize <= sizeof(TRDP_TRAIN_DIR_T))
            {
                UINT32 i;
                ttiStoreTrnDir(appHandle, pData);
                /* Request consist infos now (fill cache)   */
                for (i = 0; i < TTI_CACHED_CONSISTS; i++)
                {
                    /* invalidate entry */
                    if (appHandle->pTTDB->cstInfo[i] != NULL)
                    {
                        vos_memFree(appHandle->pTTDB->cstInfo[i]);
                        appHandle->pTTDB->cstInfo[i] = NULL;
                    }
                    ;
                }
                for (i = 0; i < TTI_CACHED_CONSISTS; i++)
                {
                    if (appHandle->pTTDB->trnDir.cstList[i].cstTopoCnt == 0)
                    {
                        break;  /* no of available consists reached   */
                    }
                    ttiRequestTTDBdata(appHandle, TTDB_STAT_CST_REQ_COMID, appHandle->pTTDB->trnDir.cstList[i].cstUUID);
                }
            }
        }
        else if (pMsg->comId == TTDB_NET_DIR_REP_COMID)
        {
            if (dataSize <= sizeof(TRDP_TRAIN_NET_DIR_T))
            {
                ttiStoreTrnNetDir(appHandle, pData);
            }
        }
        else if (pMsg->comId == TTDB_READ_CMPLT_REP_COMID)
        {
            if (dataSize <= sizeof(TRDP_READ_COMPLETE_REPLY_T))
            {
                TRDP_READ_COMPLETE_REPLY_T *pTelegram = (TRDP_READ_COMPLETE_REPLY_T *) pData;
                UINT32 crc;

                /* Handle the op_state  */

                /* check the crc:   */
                crc = vos_crc32(0xFFFFFFFF, (const UINT8 *) &pTelegram->state, dataSize - 4);
                if (crc != MAKE_LE(pTelegram->state.crc))
                {
                    vos_printLog(VOS_LOG_WARNING, "CRC error of received operational status info (%08x != %08x)!\n",
                                 crc, vos_ntohl(pTelegram->state.crc))
                        (void) tlc_setOpTrainTopoCount(appHandle, 0);
                    return;
                }
                memcpy(&appHandle->pTTDB->opTrnState.state, &pTelegram->state,
                       (dataSize > sizeof(TRDP_OP_TRAIN_DIR_STATE_T)) ? sizeof(TRDP_OP_TRAIN_DIR_STATE_T) : dataSize);

                /* unmarshall manually:   */
                appHandle->pTTDB->opTrnState.state.opTrnTopoCnt = vos_ntohl(pTelegram->state.opTrnTopoCnt);
                (void) tlc_setOpTrainTopoCount(appHandle, appHandle->pTTDB->opTrnState.state.opTrnTopoCnt);
                appHandle->pTTDB->opTrnState.state.crc = MAKE_LE(pTelegram->state.crc);

                /* handle the other parts of the message   1 */
                (void) ttiStoreOpTrnDir(appHandle, (UINT8 *) &pTelegram->opTrnDir);
                ttiStoreTrnDir(appHandle, (UINT8 *) &pTelegram->trnDir);
                ttiStoreTrnNetDir(appHandle, (UINT8 *) &pTelegram->trnNetDir);
            }
        }
        else if (pMsg->comId == TTDB_STAT_CST_REP_COMID)
        {
            UINT32 crc;
            /* check the cstTopoCnt:   */
            crc = vos_sc32(0xFFFFFFFF, pData, dataSize - 4);
            if (crc == 0u)
            {
                crc = 0xFFFFFFFF;
            }
            if (crc == vos_ntohl(*(UINT32 *)(pData + dataSize - 4)))
            {
                /* find a free place in the cache, or overwrite oldest entry   */
                (void) ttiStoreCstInfo(appHandle, pData, dataSize);
            }
            else
            {
                vos_printLog(VOS_LOG_WARNING, "CRC error of received consist info (%08x != %08x)!\n",
                             crc, vos_ntohl(*(UINT32 *)(pData + dataSize - 4)));
                return;
            }
        }
    }
    else
    {
        vos_printLog(VOS_LOG_WARNING, "Unsolicited message received (pMsg->comId %u)!\n", pMsg->comId);
        (void) tlc_setOpTrainTopoCount(appHandle, 0);
        return;

    }
}

/**********************************************************************************************************************/
/**    Function to request TTDB data from ECSP
 *
 *  Request update of our data store
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[in]      comID           Communication ID of request
 *  @param[in]      cstUUID         Pointer to the additional info
 *
 *  @retval         none
 *
 */
static void ttiRequestTTDBdata (
    TRDP_APP_SESSION_T  appHandle,
    UINT32              comID,
    const TRDP_UUID_T   cstUUID)
{
    switch (comID)
    {
        case TTDB_OP_DIR_INFO_REQ_COMID:
        {
            UINT8 param = 0;
            (void) tlm_request(appHandle, NULL, ttiMDCallback, NULL, TTDB_OP_DIR_INFO_REQ_COMID, appHandle->etbTopoCnt,
                               appHandle->opTrnTopoCnt, 0, tau_ipFromURI(appHandle,
                                                                     TTDB_OP_DIR_INFO_REQ_URI), TRDP_FLAGS_CALLBACK, 1,
                               TTDB_OP_DIR_INFO_REQ_TO_US, NULL, &param, sizeof(param), NULL, NULL);
            /* Make sure the request is sent: */
        }
        break;
        case TTDB_TRN_DIR_REQ_COMID:
        {
            UINT8 param = 0;        /* ETB0 */
            (void) tlm_request(appHandle, NULL, ttiMDCallback, NULL, TTDB_TRN_DIR_REQ_COMID, appHandle->etbTopoCnt,
                               appHandle->opTrnTopoCnt, 0, tau_ipFromURI(appHandle,
                                                                     TTDB_TRN_DIR_REQ_URI), TRDP_FLAGS_CALLBACK, 1,
                               TTDB_TRN_DIR_REQ_TO_US, NULL, &param, sizeof(param), NULL, NULL);
        }
        break;
        case TTDB_NET_DIR_REQ_COMID:
        {
            UINT8 param = 0;        /* ETB0 */
            (void) tlm_request(appHandle, NULL, ttiMDCallback, NULL, TTDB_NET_DIR_REQ_COMID, appHandle->etbTopoCnt,
                               appHandle->opTrnTopoCnt, 0, tau_ipFromURI(appHandle,
                                                                     TTDB_NET_DIR_REQ_URI), TRDP_FLAGS_CALLBACK, 1,
                               TTDB_NET_DIR_REQ_TO_US, NULL, &param, sizeof(param), NULL, NULL);
        }
        break;
        case TTDB_READ_CMPLT_REQ_COMID:
        {
            UINT8 param = 0;        /* ETB0 */
            (void) tlm_request(appHandle, NULL, ttiMDCallback, NULL, TTDB_READ_CMPLT_REQ_COMID, appHandle->etbTopoCnt,
                               appHandle->opTrnTopoCnt, 0, tau_ipFromURI(appHandle,
                                                                     TTDB_READ_CMPLT_REQ_URI), TRDP_FLAGS_CALLBACK, 1,
                               TTDB_READ_CMPLT_REQ_TO_US, NULL, &param, sizeof(param), NULL, NULL);
        }
        break;
        case TTDB_STAT_CST_REQ_COMID:
        {
            (void) tlm_request(appHandle, NULL, ttiMDCallback, NULL, TTDB_STAT_CST_REQ_COMID, appHandle->etbTopoCnt,
                               appHandle->opTrnTopoCnt, 0, tau_ipFromURI(appHandle,
                                                                     TTDB_STAT_CST_REQ_URI), TRDP_FLAGS_CALLBACK, 1,
                               TTDB_STAT_CST_REQ_TO_US, NULL, cstUUID, sizeof(TRDP_UUID_T), NULL, NULL);
        }
        break;

    }
    /* Make sure the request is sent: */
    (void) tlc_process(appHandle, NULL, NULL);
}

#pragma mark ----------------------- Public -----------------------------

/**********************************************************************************************************************/
/*    Train configuration information access                                                                          */
/**********************************************************************************************************************/

/**********************************************************************************************************************/
/**    Function to init TTI access
 *
 *  Subscribe to necessary process data for correct ECSP handling, further calls need DNS!
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[in]      userAction      Semaphore to fire if inauguration took place.
 *  @param[in]      ecspIpAddr      ECSP IP address. Currently not used.
 *  @param[in]      hostsFileName   Optional host file name as ECSP replacement. Currently not implemented.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_INIT_ERR   initialisation error
 *
 */
EXT_DECL TRDP_ERR_T tau_initTTIaccess (
    TRDP_APP_SESSION_T  appHandle,
    VOS_SEMA_T          userAction,
    TRDP_IP_ADDR_T      ecspIpAddr,
    CHAR8               *hostsFileName)
{
    if ((appHandle == NULL) || (appHandle->pTTDB != NULL))
    {
        return TRDP_INIT_ERR;
    }

    ecspIpAddr      = ecspIpAddr;
    hostsFileName   = hostsFileName;

    appHandle->pTTDB = (TAU_TTDB_T *) vos_memAlloc(sizeof(TAU_TTDB_T));
    if (appHandle->pTTDB == NULL)
    {
        return TRDP_MEM_ERR;
    }

    /*  subscribe to PD 100 */

    if (tlp_subscribe(appHandle,
                      &appHandle->pTTDB->pd100SubHandle1,
                      userAction, ttiPDCallback,
                      0u,
                      TRDP_TTDB_OP_TRN_DIR_STAT_INF_COMID,
                      0u, 0u,
                      VOS_INADDR_ANY, VOS_INADDR_ANY,
                      vos_dottedIP(TTDB_STATUS_DEST_IP),
                      (TRDP_FLAGS_T) (TRDP_FLAGS_CALLBACK | TRDP_FLAGS_FORCE_CB),
                      NULL,                      /*    default interface                    */
                      TTDB_STATUS_TO_US,
                      TRDP_TO_SET_TO_ZERO) != TRDP_NO_ERR)
    {
        vos_memFree(appHandle->pTTDB);
        return TRDP_INIT_ERR;
    }

    if (tlp_subscribe(appHandle,
                      &appHandle->pTTDB->pd100SubHandle2,
                      userAction, ttiPDCallback,
                      0u,
                      TRDP_TTDB_OP_TRN_DIR_STAT_INF_COMID,
                      0u, 0u,
                      VOS_INADDR_ANY, VOS_INADDR_ANY,
                      vos_dottedIP(TTDB_STATUS_DEST_IP_ETB0),
                      (TRDP_FLAGS_T) (TRDP_FLAGS_CALLBACK | TRDP_FLAGS_FORCE_CB),
                      NULL,                      /*    default interface                    */
                      TTDB_STATUS_TO_US,
                      TRDP_TO_SET_TO_ZERO) != TRDP_NO_ERR)
    {
        (void) tlp_unsubscribe(appHandle, appHandle->pTTDB->pd100SubHandle1);
        vos_memFree(appHandle->pTTDB);
        return TRDP_INIT_ERR;
    }

    /*  Listen for MD 101 */

    if (tlm_addListener(appHandle,
                        &appHandle->pTTDB->md101Listener1,
                        userAction,
                        ttiMDCallback,
                        TRUE,
                        TTDB_OP_DIR_INFO_COMID,
                        0,
                        0,
                        VOS_INADDR_ANY, VOS_INADDR_ANY,
                        vos_dottedIP(TTDB_OP_DIR_INFO_IP),
                        TRDP_FLAGS_CALLBACK, NULL, NULL) != TRDP_NO_ERR)
    {
        (void) tlp_unsubscribe(appHandle, appHandle->pTTDB->pd100SubHandle1);
        (void) tlp_unsubscribe(appHandle, appHandle->pTTDB->pd100SubHandle2);
        vos_memFree(appHandle->pTTDB);
        return TRDP_INIT_ERR;
    }

    if (tlm_addListener(appHandle,
                        &appHandle->pTTDB->md101Listener2,
                        userAction,
                        ttiMDCallback,
                        TRUE,
                        TTDB_OP_DIR_INFO_COMID,
                        0,
                        0,
                        VOS_INADDR_ANY, VOS_INADDR_ANY,
                        vos_dottedIP(TTDB_OP_DIR_INFO_IP_ETB0),
                        TRDP_FLAGS_CALLBACK, NULL, NULL) != TRDP_NO_ERR)
    {
        (void) tlp_unsubscribe(appHandle, appHandle->pTTDB->pd100SubHandle1);
        (void) tlp_unsubscribe(appHandle, appHandle->pTTDB->pd100SubHandle2);
        (void) tlm_delListener(appHandle, appHandle->pTTDB->md101Listener1);
        vos_memFree(appHandle->pTTDB);
        return TRDP_INIT_ERR;
    }
    return TRDP_NO_ERR;
}

/**********************************************************************************************************************/
/**    Release any resources allocated by TTI
 *  Must be called before closing the session.
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *
 *  @retval         none
 *
 */
EXT_DECL void tau_deInitTTI (
    TRDP_APP_SESSION_T appHandle)
{
    if (appHandle->pTTDB != NULL)
    {
        UINT32 i;
        for (i = 0; i < appHandle->pTTDB->noOfCachedCst; i++)
        {
            if (appHandle->pTTDB->cstInfo[i] != NULL)
            {
                vos_memFree(appHandle->pTTDB->cstInfo[i]);
                appHandle->pTTDB->cstInfo[i] = NULL;
            }
        }
        (void) tlm_delListener(appHandle, appHandle->pTTDB->md101Listener1);
        (void) tlp_unsubscribe(appHandle, appHandle->pTTDB->pd100SubHandle1);
        (void) tlm_delListener(appHandle, appHandle->pTTDB->md101Listener2);
        (void) tlp_unsubscribe(appHandle, appHandle->pTTDB->pd100SubHandle2);
        vos_memFree(appHandle->pTTDB);
        appHandle->pTTDB = NULL;
    }
}

/**********************************************************************************************************************/
/**    Function to retrieve the operational train directory state.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pOpTrnDirState  Pointer to an operational train directory state structure to be returned.
 *  @param[out]     pOpTrnDir       Pointer to an operational train directory structure to be returned.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *  @retval         TRDP_NODATA_ERR Data currently not available, try again later
 *
 */
EXT_DECL TRDP_ERR_T tau_getOpTrDirectory (
    TRDP_APP_SESSION_T          appHandle,
    TRDP_OP_TRAIN_DIR_STATE_T   *pOpTrnDirState,
    TRDP_OP_TRAIN_DIR_T         *pOpTrnDir)
{
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL))
    {
        return TRDP_PARAM_ERR;
    }
    if ((appHandle->pTTDB->opTrnDir.opCstCnt == 0) ||
        (appHandle->pTTDB->opTrnDir.opTrnTopoCnt != appHandle->opTrnTopoCnt))    /* need update? */
    {
        ttiRequestTTDBdata(appHandle, TTDB_OP_DIR_INFO_REQ_COMID, NULL);
        return TRDP_NODATA_ERR;
    }
    if (pOpTrnDirState != NULL)
    {
        *pOpTrnDirState = appHandle->pTTDB->opTrnState.state;
    }
    if (pOpTrnDir != NULL)
    {
        *pOpTrnDir = appHandle->pTTDB->opTrnDir;
    }
    return TRDP_NO_ERR;
}

/**********************************************************************************************************************/
/**    Function to retrieve the operational train directory state info.
 *  Return a copy of the last received PD 100 telegram.
 *  Note: The values are in host endianess! When validating (
 v2), network endianess must be ensured.
 *
 *  @param[in]      appHandle               Handle returned by tlc_openSession().
 *  @param[out]     pOpTrnDirStatusInfo     Pointer to an operational train directory state structure to be returned.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *
 */
EXT_DECL TRDP_ERR_T tau_getOpTrnDirectoryStatusInfo (
    TRDP_APP_SESSION_T              appHandle,
    TRDP_OP_TRAIN_DIR_STATUS_INFO_T *pOpTrnDirStatusInfo)
{
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pOpTrnDirStatusInfo == NULL))
    {
        return TRDP_PARAM_ERR;
    }
    *pOpTrnDirStatusInfo = appHandle->pTTDB->opTrnState;
    return TRDP_NO_ERR;
}


/**********************************************************************************************************************/
/**    Function to retrieve the train directory.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pTrnDir         Pointer to a train directory structure to be returned.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *  @retval         TRDP_NODATA_ERR Try later
 *
 */
EXT_DECL TRDP_ERR_T tau_getTrDirectory (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_TRAIN_DIR_T    *pTrnDir)
{
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pTrnDir == NULL))
    {
        return TRDP_PARAM_ERR;
    }
    if (appHandle->pTTDB->trnDir.cstCnt == 0 ||
        appHandle->pTTDB->trnDir.trnTopoCnt != appHandle->etbTopoCnt)     /* need update? */
    {
        ttiRequestTTDBdata(appHandle, TTDB_TRN_DIR_REQ_COMID, NULL);
        return TRDP_NODATA_ERR;
    }
    *pTrnDir = appHandle->pTTDB->trnDir;
    return TRDP_NO_ERR;
}


/**********************************************************************************************************************/
/**    Function to retrieve the consist info.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pCstInfo        Pointer to a consist info structure to be returned.
 *  @param[in]      cstUUID         UUID of the consist the consist info is rquested for.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *
 */
EXT_DECL TRDP_ERR_T tau_getStaticCstInfo (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_CONSIST_INFO_T *pCstInfo,
    TRDP_UUID_T const   cstUUID)
{
    UINT32 l_index;
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pCstInfo == NULL))
    {
        return TRDP_PARAM_ERR;
    }

    if (cstUUID == NULL)
    {
        l_index = 0;
    }
    else
    {
        /* find the consist in our cache list */
        for (l_index = 0; l_index < TTI_CACHED_CONSISTS; l_index++)
        {
            if (appHandle->pTTDB->cstInfo[l_index] != NULL &&
                memcmp(appHandle->pTTDB->cstInfo[l_index]->cstUUID, cstUUID, sizeof(TRDP_UUID_T)) == 0)
            {
                break;
            }
        }
    }
    if (l_index < TTI_CACHED_CONSISTS &&
        appHandle->pTTDB->cstInfo[l_index] != NULL)
    {
        memcpy(pCstInfo, appHandle->pTTDB->cstInfo[l_index], appHandle->pTTDB->cstSize[l_index]);
    }
    else    /* not found, get it and return directly */
    {
        ttiRequestTTDBdata(appHandle, TTDB_STAT_CST_REQ_COMID, cstUUID);
        return TRDP_NODATA_ERR;
    }
    return TRDP_NO_ERR;
}


/**********************************************************************************************************************/
/**    Function to retrieve the operational train directory.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pOpTrnDirState   Pointer to an operational train directory state structure to be returned.
 *  @param[out]     pOpTrnDir        Pointer to an operational train directory structure to be returned.
 *  @param[out]     pTrnDir          Pointer to a train directory structure to be returned.
 *  @param[out]     pTrnNetDir       Pointer to a train network directory structure to be returned.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *
 */
EXT_DECL TRDP_ERR_T tau_getTTI (
    TRDP_APP_SESSION_T          appHandle,
    TRDP_OP_TRAIN_DIR_STATE_T   *pOpTrnDirState,
    TRDP_OP_TRAIN_DIR_T         *pOpTrnDir,
    TRDP_TRAIN_DIR_T            *pTrnDir,
    TRDP_TRAIN_NET_DIR_T        *pTrnNetDir)
{
    if (appHandle == NULL ||
        appHandle->pTTDB == NULL)
    {
        return TRDP_PARAM_ERR;
    }

    if (pOpTrnDirState != NULL)
    {
        *pOpTrnDirState = appHandle->pTTDB->opTrnState.state;
    }
    if (pOpTrnDir != NULL)
    {
        *pOpTrnDir = appHandle->pTTDB->opTrnDir;
    }
    if (pTrnDir != NULL)
    {
        *pTrnDir = appHandle->pTTDB->trnDir;
    }
    if (pTrnNetDir != NULL)
    {
        *pTrnNetDir = appHandle->pTTDB->trnNetDir;
    }
    return TRDP_NO_ERR;
}


/**********************************************************************************************************************/
/**    Function to retrieve the total number of consists in the train.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pTrnCstCnt      Pointer to the number of consists to be returned
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *  @retval         TRDP_NODATA_ERR Try again
 *
 */
EXT_DECL TRDP_ERR_T tau_getTrnCstCnt (
    TRDP_APP_SESSION_T  appHandle,
    UINT16              *pTrnCstCnt)
{
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pTrnCstCnt == NULL))
    {
        return TRDP_PARAM_ERR;
    }
    if (appHandle->pTTDB->trnDir.cstCnt == 0 ||
        appHandle->pTTDB->opTrnState.etbTopoCnt != appHandle->etbTopoCnt)     /* need update? */
    {
        ttiRequestTTDBdata(appHandle, TTDB_TRN_DIR_REQ_COMID, NULL);
        return TRDP_NODATA_ERR;
    }

    *pTrnCstCnt = appHandle->pTTDB->trnDir.cstCnt;
    return TRDP_NO_ERR;
}


/**********************************************************************************************************************/
/**    Function to retrieve the total number of vehicles in the train.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pTrnVehCnt      Pointer to the number of vehicles to be returned
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *  @retval         TRDP_NODATA_ERR Try again
 *
 */
EXT_DECL TRDP_ERR_T tau_getTrnVehCnt (
    TRDP_APP_SESSION_T  appHandle,
    UINT16              *pTrnVehCnt)
{
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pTrnVehCnt == NULL))
    {
        return TRDP_PARAM_ERR;
    }

    if (appHandle->pTTDB->opTrnDir.opCstCnt == 0 ||
        appHandle->pTTDB->opTrnDir.opVehCnt == 0 ||
        appHandle->pTTDB->opTrnState.etbTopoCnt != appHandle->etbTopoCnt)     /* need update? */
    {
        ttiRequestTTDBdata(appHandle, TTDB_OP_DIR_INFO_REQ_COMID, NULL);
        return TRDP_NODATA_ERR;
    }

    *pTrnVehCnt = appHandle->pTTDB->opTrnDir.opVehCnt;
    return TRDP_NO_ERR;
}


/**********************************************************************************************************************/
/**    Function to retrieve the total number of vehicles in a consist.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pCstVehCnt      Pointer to the number of vehicles to be returned
 *  @param[in]      pCstLabel       Pointer to a consist label. NULL means own consist.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *  @retval         TRDP_NODATA_ERR Try again
 *
 */
EXT_DECL TRDP_ERR_T tau_getCstVehCnt (
    TRDP_APP_SESSION_T  appHandle,
    UINT16              *pCstVehCnt,
    const TRDP_LABEL_T  pCstLabel)
{
    UINT32 l_index;
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pCstVehCnt == NULL))
    {
        return TRDP_PARAM_ERR;
    }

    if (pCstLabel == NULL)
    {
        l_index = 0;
    }
    else
    {
        /* find the consist in our cache list */
        for (l_index = 0; l_index < TTI_CACHED_CONSISTS; l_index++)
        {
            if (appHandle->pTTDB->cstInfo[l_index] != NULL &&
                vos_strnicmp(appHandle->pTTDB->cstInfo[l_index]->cstId, pCstLabel, sizeof(TRDP_NET_LABEL_T)) == 0)
            {
                break;
            }
        }
    }
    if (l_index < TTI_CACHED_CONSISTS)
    {
        *pCstVehCnt = vos_ntohs(appHandle->pTTDB->cstInfo[l_index]->vehCnt);
    }
    else    /* not found, get it and return directly */
    {
        TRDP_UUID_T cstUUID;
        ttiGetUUIDfromLabel(appHandle, cstUUID, pCstLabel);
        ttiRequestTTDBdata(appHandle, TTDB_STAT_CST_REQ_COMID, cstUUID);
        return TRDP_NODATA_ERR;
    }
    return TRDP_NO_ERR;
}


/**********************************************************************************************************************/
/**    Function to retrieve the total number of functions in a consist.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pCstFctCnt      Pointer to the number of functions to be returned
 *  @param[in]      pCstLabel       Pointer to a consist label. NULL means own consist.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *
 */
EXT_DECL TRDP_ERR_T tau_getCstFctCnt (
    TRDP_APP_SESSION_T  appHandle,
    UINT16              *pCstFctCnt,
    const TRDP_LABEL_T  pCstLabel)
{
    UINT32 l_index;
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pCstFctCnt == NULL))
    {
        return TRDP_PARAM_ERR;
    }

    if (pCstLabel == NULL)
    {
        l_index = 0;
    }
    else
    {
        /* find the consist in our cache list */
        for (l_index = 0; l_index < TTI_CACHED_CONSISTS; l_index++)
        {
            if (appHandle->pTTDB->cstInfo[l_index] != NULL &&
                vos_strnicmp(appHandle->pTTDB->cstInfo[l_index]->cstId, pCstLabel, sizeof(TRDP_NET_LABEL_T)) == 0)
            {
                break;
            }
        }
    }
    if (l_index < TTI_CACHED_CONSISTS)
    {
        *pCstFctCnt = vos_ntohs(appHandle->pTTDB->cstInfo[l_index]->fctCnt);
    }
    else    /* not found, get it and return directly */
    {
        TRDP_UUID_T cstUUID;
        ttiGetUUIDfromLabel(appHandle, cstUUID, pCstLabel);
        ttiRequestTTDBdata(appHandle, TTDB_STAT_CST_REQ_COMID, cstUUID);
        return TRDP_NODATA_ERR;
    }
    return TRDP_NO_ERR;
}


/* ---------------------------------------------------------------------------- */

/**********************************************************************************************************************/
/**    Function to retrieve the function information of the consist.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pFctInfo        Pointer to function info list to be returned.
 *                                  Memory needs to be provided by application. Set NULL if not used.
 *  @param[in]      pCstLabel       Pointer to a consist label. NULL means own consist.
 *  @param[in]      maxFctCnt       Maximal number of functions to be returned in provided buffer.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *
 */
EXT_DECL TRDP_ERR_T tau_getCstFctInfo (
    TRDP_APP_SESSION_T      appHandle,
    TRDP_FUNCTION_INFO_T    *pFctInfo,
    const TRDP_LABEL_T      pCstLabel,
    UINT16                  maxFctCnt)
{
    UINT32 l_index, l_index2;
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pFctInfo == NULL) ||
        (maxFctCnt == 0))
    {
        return TRDP_PARAM_ERR;
    }

    if (pCstLabel == NULL)
    {
        l_index = 0;
    }
    else
    {
        /* find the consist in our cache list */
        for (l_index = 0; l_index < TTI_CACHED_CONSISTS; l_index++)
        {
            if (appHandle->pTTDB->cstInfo[l_index] != NULL &&
                vos_strnicmp(appHandle->pTTDB->cstInfo[l_index]->cstId, pCstLabel, sizeof(TRDP_NET_LABEL_T)) == 0)
            {
                break;
            }
        }
    }
    if (l_index < TTI_CACHED_CONSISTS)
    {
        for (l_index2 = 0; l_index2 < vos_ntohs(appHandle->pTTDB->cstInfo[l_index]->fctCnt) &&
             l_index2 < maxFctCnt; ++l_index2)
        {
            pFctInfo[l_index2]          = appHandle->pTTDB->cstInfo[l_index]->pFctInfoList[l_index2];
            pFctInfo[l_index2].fctId    = vos_ntohs(pFctInfo[l_index2].fctId);
        }
    }
    else    /* not found, get it and return directly */
    {
        TRDP_UUID_T cstUUID;
        ttiGetUUIDfromLabel(appHandle, cstUUID, pCstLabel);
        ttiRequestTTDBdata(appHandle, TTDB_STAT_CST_REQ_COMID, cstUUID);
        return TRDP_NODATA_ERR;
    }
    return TRDP_NO_ERR;
}


/**********************************************************************************************************************/
/**    Function to retrieve the vehicle information of a consist's vehicle.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pVehInfo        Pointer to the vehicle info to be returned.
 *  @param[in]      pVehLabel       Pointer to a vehicle label. NULL means own vehicle  if cstLabel refers to own consist.
 *  @param[in]      pCstLabel       Pointer to a consist label. NULL means own consist.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *
 */
EXT_DECL TRDP_ERR_T tau_getVehInfo (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_VEHICLE_INFO_T *pVehInfo,
    const TRDP_LABEL_T  pVehLabel,
    const TRDP_LABEL_T  pCstLabel)
{
    UINT32 l_index, l_index2;
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pVehInfo == NULL))
    {
        return TRDP_PARAM_ERR;
    }

    if (pCstLabel == NULL)
    {
        l_index = 0;
    }
    else
    {
        /* find the consist in our cache list */
        for (l_index = 0; l_index < TTI_CACHED_CONSISTS; l_index++)
        {
            if (appHandle->pTTDB->cstInfo[l_index] != NULL &&
                vos_strnicmp(appHandle->pTTDB->cstInfo[l_index]->cstId, pCstLabel, sizeof(TRDP_NET_LABEL_T)) == 0)
            {
                break;
            }
        }
    }
    if (l_index < TTI_CACHED_CONSISTS)
    {
        for (l_index2 = 0; l_index2 < vos_ntohs(appHandle->pTTDB->cstInfo[l_index]->vehCnt); ++l_index2)
        {
            if (pVehLabel == NULL ||
                vos_strnicmp(pVehLabel, appHandle->pTTDB->cstInfo[l_index]->pVehInfoList[l_index2].vehId,
                             sizeof(TRDP_NET_LABEL_T)) == 0)
            {
                *pVehInfo = appHandle->pTTDB->cstInfo[l_index]->pVehInfoList[l_index2];
            }
        }
    }
    else    /* not found, get it and return directly */
    {
        TRDP_UUID_T cstUUID;
        ttiGetUUIDfromLabel(appHandle, cstUUID, pCstLabel);
        ttiRequestTTDBdata(appHandle, TTDB_STAT_CST_REQ_COMID, cstUUID);
        return TRDP_NODATA_ERR;
    }
    return TRDP_NO_ERR;
}


/**********************************************************************************************************************/
/**    Function to retrieve the consist information of a train's consist.
 *
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pCstInfo        Pointer to the consist info to be returned.
 *  @param[in]      pCstLabel       Pointer to a consist label. NULL means own consist.
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *
 */
EXT_DECL TRDP_ERR_T tau_getCstInfo (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_CONSIST_INFO_T *pCstInfo,
    const TRDP_LABEL_T  pCstLabel)
{
    UINT32 l_index;
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pCstInfo == NULL))
    {
        return TRDP_PARAM_ERR;
    }

    if (pCstLabel == NULL)
    {
        l_index = 0;
    }
    else
    {
        /* find the consist in our cache list */
        for (l_index = 0; l_index < TTI_CACHED_CONSISTS; l_index++)
        {
            if (appHandle->pTTDB->cstInfo[l_index] != NULL &&
                vos_strnicmp(appHandle->pTTDB->cstInfo[l_index]->cstId, pCstLabel, sizeof(TRDP_NET_LABEL_T)) == 0)
            {
                break;
            }
        }
    }
    if (l_index < TTI_CACHED_CONSISTS)
    {
        *pCstInfo           = *appHandle->pTTDB->cstInfo[l_index];
        pCstInfo->etbCnt    = vos_ntohs(pCstInfo->etbCnt);
        pCstInfo->vehCnt    = vos_ntohs(pCstInfo->vehCnt);
        pCstInfo->fctCnt    = vos_ntohs(pCstInfo->fctCnt);
    }
    else    /* not found, get it and return directly */
    {
        TRDP_UUID_T cstUUID;
        ttiGetUUIDfromLabel(appHandle, cstUUID, pCstLabel);
        ttiRequestTTDBdata(appHandle, TTDB_STAT_CST_REQ_COMID, cstUUID);
        return TRDP_NODATA_ERR;
    }
    return TRDP_NO_ERR;
}


/* ---------------------------------------------------------------------------- */


/**********************************************************************************************************************/
/**    Function to retrieve the orientation of the given vehicle.
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession().
 *  @param[out]     pVehOrient      Pointer to the vehicle orientation to be returned
 *                                   '00'B = not known (corrected vehicle)
 *                                   '01'B = same as operational train direction
 *                                   '10'B = inverse to operational train direction
 *  @param[out]     pCstOrient      Pointer to the consist orientation to be returned
 *                                   '00'B = not known (corrected vehicle)
 *                                   '01'B = same as operational train direction
 *                                   '10'B = inverse to operational train direction
 *  @param[in]      pVehLabel       vehLabel = NULL means own vehicle if cstLabel == NULL, currently ignored.
 *  @param[in]      pCstLabel       cstLabel = NULL means own consist
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *
 */
EXT_DECL TRDP_ERR_T tau_getVehOrient (
    TRDP_APP_SESSION_T  appHandle,
    UINT8               *pVehOrient,
    UINT8               *pCstOrient,
    TRDP_LABEL_T        pVehLabel,
    TRDP_LABEL_T        pCstLabel)
{
    UINT32 l_index, l_index2, l_index3;

    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL) ||
        (pVehOrient == NULL) ||
        (pCstOrient == NULL))
    {
        return TRDP_PARAM_ERR;
    }

    pVehLabel = pVehLabel;

    *pVehOrient = 0;
    *pCstOrient = 0;

    if (pCstLabel == NULL)
    {
        l_index = 0;
    }
    else
    {
        /* find the consist in our cache list */
        for (l_index = 0; l_index < TTI_CACHED_CONSISTS; l_index++)
        {
            if (appHandle->pTTDB->cstInfo[l_index] != NULL &&
                vos_strnicmp(appHandle->pTTDB->cstInfo[l_index]->cstId, pCstLabel, sizeof(TRDP_NET_LABEL_T)) == 0)
            {
                break;
            }
        }
    }
    if (l_index < TTI_CACHED_CONSISTS)
    {
        /* Search the vehicles in the OP_TRAIN_DIR for a matching vehID */

        for (l_index2 = 0; l_index2 < appHandle->pTTDB->opTrnDir.opCstCnt; l_index2++)
        {
            if (vos_strnicmp((CHAR8 *) appHandle->pTTDB->opTrnDir.opCstList[l_index2].cstUUID,
                             (CHAR8 *) appHandle->pTTDB->cstInfo[l_index]->cstUUID, sizeof(TRDP_UUID_T)) == 0)
            {
                /* consist found   */
                *pCstOrient = appHandle->pTTDB->opTrnDir.opCstList[l_index2].opCstOrient;

                for (l_index3 = 0; l_index3 < appHandle->pTTDB->opTrnDir.opVehCnt; l_index3++)
                {
                    if (appHandle->pTTDB->opTrnDir.opVehList[l_index3].ownOpCstNo ==
                        appHandle->pTTDB->opTrnDir.opCstList[l_index2].opCstNo)
                    {
                        *pVehOrient = appHandle->pTTDB->opTrnDir.opVehList[l_index3].vehOrient;
                        return TRDP_NO_ERR;
                    }
                }
            }
        }
    }
    else    /* not found, get it and return directly */
    {
        TRDP_UUID_T cstUUID;
        ttiGetUUIDfromLabel(appHandle, cstUUID, pCstLabel);
        ttiRequestTTDBdata(appHandle, TTDB_STAT_CST_REQ_COMID, cstUUID);
        return TRDP_NODATA_ERR;
    }
    return TRDP_NO_ERR;
}

/**********************************************************************************************************************/
/**    Who am I ?.
 *  Realizes a kind of 'Who am I' function. It is used to determine the own identifiers (i.e. the own labels),
 *  which may be used as host part of the own fully qualified domain name.
 *
 *  @param[in]      appHandle       Handle returned by tlc_openSession()
 *  @param[out]     pDevId          Returns the device label (host name)
 *  @param[out]     pVehId          Returns the vehicle label
 *  @param[out]     pCstId          Returns the consist label
 *
 *  @retval         TRDP_NO_ERR     no error
 *  @retval         TRDP_PARAM_ERR  Parameter error
 *  @retval         TRDP_NODATA_ERR Data currently not available, call again
 *
 */
EXT_DECL TRDP_ERR_T tau_getOwnIds (
    TRDP_APP_SESSION_T  appHandle,
    TRDP_LABEL_T        *pDevId,
    TRDP_LABEL_T        *pVehId,
    TRDP_LABEL_T        *pCstId)
{
    if ((appHandle == NULL) ||
        (appHandle->pTTDB == NULL))
    {
        return TRDP_PARAM_ERR;
    }
    /* if not already there, get the network directory */
    if ((appHandle->pTTDB->trnNetDir.entryCnt == 0) ||
        (appHandle->pTTDB->opTrnState.ownTrnCstNo == 0))            /* from PD 100  */
    {    /* not found, get it and return immediately */
        ttiRequestTTDBdata(appHandle, TTDB_NET_DIR_REQ_COMID, NULL);
        return TRDP_NODATA_ERR;
    }

    /* if not already there, get the consist info for our consist */
    if (appHandle->pTTDB->noOfCachedCst == 0)                 /* own Consist info  */
    {    /* not found, get it and return immediately */
        ttiRequestTTDBdata(appHandle, TRDP_TTDB_STATIC_CST_INF_REQ_COMID,
                           appHandle->pTTDB->trnNetDir.trnNetDir[appHandle->pTTDB->opTrnState.ownTrnCstNo - 1].cstUUID);
        return TRDP_NODATA_ERR;
    }

    /* here we should have all the infos we need to fullfill the request */

    if (pDevId != NULL)
    {
        unsigned int    index;
        /* deduct our device / function ID from our IP address */
        UINT16          ownIP = (UINT16) (appHandle->realIP & 0x00000FFF);
        /* Problem: What if it is not set? Default interface is 0! */

        /* we traverse the consist info's functions */
        for (index = 0; index < appHandle->pTTDB->cstInfo[0]->fctCnt; index++)
        {
            if (ownIP == appHandle->pTTDB->cstInfo[0]->pFctInfoList[index].fctId)
            {
                /* Get the name */
                if (pDevId != NULL)
                {
                    memcpy(pDevId, appHandle->pTTDB->cstInfo[0]->pFctInfoList[index].fctName, TRDP_MAX_LABEL_LEN);
                }

                /* Get the vehicle name this device is in */
                if (pVehId != NULL)
                {
                    UINT8 vehNo = appHandle->pTTDB->cstInfo[0]->pFctInfoList[index].cstVehNo;
                    memcpy(pVehId, appHandle->pTTDB->cstInfo[0]->pVehInfoList[vehNo].vehId, TRDP_MAX_LABEL_LEN);
                }
                break;
            }
        }
    }
    /* Get the consist label (UIC identifier) */
    if (pCstId != NULL)
    {
        memcpy(pCstId, appHandle->pTTDB->cstInfo[0]->cstId, TRDP_MAX_LABEL_LEN);
    }
    return TRDP_NO_ERR;
}

/**********************************************************************************************************************/
/** Get own operational consist number.
 *
 *  @param[in]      appHandle           The handle returned by tlc_init
 *
 *  @retval         ownOpCstNo          own operational consist number value
 *                  0                   on error
 */
EXT_DECL UINT8 tau_getOwnOpCstNo (
    TRDP_APP_SESSION_T appHandle)
{
    if ((appHandle != NULL) &&
        (appHandle->pTTDB != NULL))
    {
        return appHandle->pTTDB->opTrnState.ownOpCstNo;    /* pTTDB opTrnState ownOpCstNo; */
    }
    return 0u;
}

/**********************************************************************************************************************/
/** Get own train consist number.
 *
 *  @param[in]      appHandle           The handle returned by tlc_init
 *
 *  @retval         ownTrnCstNo         own train consist number value
 *                  0                   on error
 */
EXT_DECL UINT8 tau_getOwnTrnCstNo (
    TRDP_APP_SESSION_T appHandle)
{
    if ((appHandle != NULL) &&
        (appHandle->pTTDB != NULL))
    {
        return appHandle->pTTDB->opTrnState.ownTrnCstNo;
    }
    return 0u;
}
