/* $Id: tftp.c 41970 2012-06-29 05:55:17Z vboxsync $ */
/** @file
 * NAT - TFTP server.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
 * tftp.c - a simple, read-only tftp server for qemu
 *
 * Copyright (c) 2004 Magnus Damm <damm@opensource.se>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <slirp.h>
#include <iprt/file.h>
#include <iprt/asm-math.h>

typedef struct TFTPOPTIONDESC
{
    const char *pszName;
    ENMTFTPSESSIONFMT enmType;
    int         cbName;
    bool        fHasValue;
} TFTPOPTIONDESC, *PTFTPOPTIONDESC;

typedef const PTFTPOPTIONDESC PCTFTPOPTIONDESC;
static TFTPOPTIONDESC g_TftpTransferFmtDesc[] =
{
    {"octet", TFTPFMT_OCTET, 5, false}, /* RFC1350 */
    {"netascii", TFTPFMT_NETASCII, 8, false}, /* RFC1350 */
    {"mail", TFTPFMT_MAIL, 4, false}, /* RFC1350 */
};

static TFTPOPTIONDESC g_TftpDesc[] =
{
    {"blksize", TFTPFMT_NOT_FMT, 7, true}, /* RFC2348 */
    {"timeout", TFTPFMT_NOT_FMT, 7, true}, /* RFC2349 */
    {"tsize", TFTPFMT_NOT_FMT, 5, true}, /* RFC2349 */
    {"size", TFTPFMT_NOT_FMT, 4, true}, /* RFC2349 */
};

static uint16_t g_au16RFC2348TftpSessionBlkSize[] =
{
    512,
    1024,
    1428,
    2048,
    4096,
    8192
};

/**
 * This function evaluate file name.
 * @param pu8Payload
 * @param cbPayload
 * @param cbFileName
 * @return VINF_SUCCESS -
 *         VERR_INVALID_PARAMETER -
 */
DECLINLINE(int) tftpSecurityFilenameCheck(PNATState pData, PCTFTPSESSION pcTftpSession)
{
    int cbSessionFilename = 0;
    int rc = VINF_SUCCESS;
    AssertPtrReturn(pcTftpSession, VERR_INVALID_PARAMETER);
    cbSessionFilename = RTStrNLen((const char *)pcTftpSession->pszFilename, TFTP_FILENAME_MAX);
    if (   !RTStrNCmp((const char*)pcTftpSession->pszFilename, "../", 3)
        || (pcTftpSession->pszFilename[cbSessionFilename - 1] == '/')
        ||  RTStrStr((const char *)pcTftpSession->pszFilename, "/../"))
        rc = VERR_FILE_NOT_FOUND;

    /* only allow exported prefixes */
    if (   RT_SUCCESS(rc)
        && !tftp_prefix)
        rc = VERR_INTERNAL_ERROR;
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/*
 * This function returns index of option descriptor in passed descriptor array
 * @param piIdxOpt returned index value
 * @param paTftpDesc array of known Tftp descriptors
 * @param caTftpDesc size of array of tftp descriptors
 * @param pszOpt name of option
 */
DECLINLINE(int) tftpFindDesciptorIndexByName(int *piIdxOpt, PCTFTPOPTIONDESC paTftpDesc, int caTftpDesc, const char *pszOptName)
{
    int rc = VINF_SUCCESS;
    int idxOption = 0;
    AssertReturn(piIdxOpt, VERR_INVALID_PARAMETER);
    AssertReturn(paTftpDesc, VERR_INVALID_PARAMETER);
    AssertReturn(pszOptName, VERR_INVALID_PARAMETER);
    for (idxOption = 0; idxOption < caTftpDesc; ++idxOption)
    {
        if (!RTStrNICmp(pszOptName, paTftpDesc[idxOption].pszName, 10))
        {
            *piIdxOpt = idxOption;
            return rc;
        }
    }
    rc = VERR_NOT_FOUND;
    return rc;
}

/**
 * Helper function to look for index of descriptor in transfer format descriptors
 * @param piIdxOpt returned value of index
 * @param pszOpt name of option
 */
DECLINLINE(int) tftpFindTransferFormatIdxbyName(int *piIdxOpt, const char *pszOpt)
{
    return tftpFindDesciptorIndexByName(piIdxOpt, &g_TftpTransferFmtDesc[0], RT_ELEMENTS(g_TftpTransferFmtDesc), pszOpt);
}

/**
 * Helper function to look for index of descriptor in options descriptors
 * @param piIdxOpt returned value of index
 * @param pszOpt name of option
 */
DECLINLINE(int) tftpFindOptionIdxbyName(int *piIdxOpt, const char *pszOpt)
{
    return tftpFindDesciptorIndexByName(piIdxOpt, &g_TftpDesc[0], RT_ELEMENTS(g_TftpDesc), pszOpt);
}


DECLINLINE(bool) tftpIsAcceptableOption(const char *pszOptionName)
{
    int idxOptDesc = 0;
    AssertPtrReturn(pszOptionName, false);
    AssertReturn(RTStrNLen(pszOptionName,10) >= 4, false);
    AssertReturn(RTStrNLen(pszOptionName,10) < 8, false);
    for(idxOptDesc = 0; idxOptDesc < RT_ELEMENTS(g_TftpTransferFmtDesc); ++idxOptDesc)
    {
        if (!RTStrNICmp(pszOptionName, g_TftpTransferFmtDesc[idxOptDesc].pszName, 10))
            return true;
    }
    for(idxOptDesc = 0; idxOptDesc < RT_ELEMENTS(g_TftpDesc); ++idxOptDesc)
    {
        if (!RTStrNICmp(pszOptionName, g_TftpDesc[idxOptDesc].pszName, 10))
            return true;
    }
    return false;
}

/**
 * This function returns the tftp transfer mode
 * @param pTftpIpHeader header of tftp (includes IP, UDP and TFTP) it's required for validating that buffer comming
 *      in pcu8Options is comes right after header.
 * @param pcu8Options pointer to options buffer
 * @param cbOptions size of the options buffer
 */
DECLINLINE(char *) tftpOptionMode(PCTFTPIPHDR pTftpIpHeader, const uint8_t *pcu8Options, int cbOptions)
{
    int idxOptDesc = 0;
    AssertPtrReturn(pTftpIpHeader, NULL);
    AssertPtrReturn(pcu8Options, NULL);
    AssertReturn(cbOptions >= 4, NULL);
    /* @todo validate that Mode Option just after filename of TFTP */
    for (idxOptDesc = 0; idxOptDesc < RT_ELEMENTS(g_TftpTransferFmtDesc); ++idxOptDesc)
    {
        if (!RTStrNICmp(g_TftpTransferFmtDesc[idxOptDesc].pszName, (const char *)pcu8Options, cbOptions))
            return (char *)g_TftpTransferFmtDesc[idxOptDesc].pszName;
    }
    return NULL;
}

/**
 * This helper function that validate if client want to operate in supported by server mode.
 * @param pcTftpHeader comulative header (IP, UDP, TFTP)
 * @param pcu8Options pointer to the options supposing that pointer points at the mode option
 * @param cbOptions size of the options buffer
 */
DECLINLINE(int) tftpIsSupportedTransferMode(PCTFTPSESSION pcTftpSession)
{
    AssertPtrReturn(pcTftpSession, 0);
    return (pcTftpSession->enmTftpFmt == TFTPFMT_OCTET);
}


DECLINLINE(void) tftpSessionUpdate(PNATState pData, PTFTPSESSION pTftpSession)
{
    pTftpSession->iTimestamp = curtime;
    pTftpSession->fInUse = 1;
}

DECLINLINE(void) tftpSessionTerminate(PTFTPSESSION pTftpSession)
{
    pTftpSession->fInUse = 0;
}

DECLINLINE(int) tftpSessionOptionParse(PTFTPSESSION pTftpSession, PCTFTPIPHDR pcTftpIpHeader)
{
    int rc = VINF_SUCCESS;
    char *pszTftpRRQRaw;
    int idxTftpRRQRaw = 0;
    int cbTftpRRQRaw = 0;
    int fWithArg = 0;
    int idxOptionArg = 0;
    AssertPtrReturn(pTftpSession, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcTftpIpHeader, VERR_INVALID_PARAMETER);
    AssertReturn(RT_N2H_U16(pcTftpIpHeader->u16TftpOpType) == TFTP_RRQ, VERR_INVALID_PARAMETER);
    LogFlowFunc(("pTftpSession:%p, pcTftpIpHeader:%p\n", pTftpSession, pcTftpIpHeader));
    pszTftpRRQRaw = (char *)&pcTftpIpHeader->Core;
    cbTftpRRQRaw = RT_H2N_U16(pcTftpIpHeader->UdpHdr.uh_ulen) + sizeof(struct ip) - RT_OFFSETOF(TFTPIPHDR, Core);
    while(cbTftpRRQRaw)
    {
        idxTftpRRQRaw = RTStrNLen(pszTftpRRQRaw, 512 - idxTftpRRQRaw) + 1;
        if (RTStrNLen((char *)pTftpSession->pszFilename, TFTP_FILENAME_MAX) == 0)
        {
            rc = RTStrCopy((char *)pTftpSession->pszFilename, TFTP_FILENAME_MAX, pszTftpRRQRaw);
            if (RT_FAILURE(rc))
            {
                LogFlowFuncLeaveRC(rc);
                AssertRCReturn(rc,rc);
            }
        }
        else if (pTftpSession->enmTftpFmt == TFTPFMT_NONE)
        {
            int idxFmt = 0;
            rc = tftpFindTransferFormatIdxbyName(&idxFmt, pszTftpRRQRaw);
            if (RT_FAILURE(rc))
            {
                LogFlowFuncLeaveRC(VERR_INTERNAL_ERROR);
                return VERR_INTERNAL_ERROR;
            }
            AssertReturn(   g_TftpTransferFmtDesc[idxFmt].enmType != TFTPFMT_NONE
                         && g_TftpTransferFmtDesc[idxFmt].enmType != TFTPFMT_NOT_FMT, VERR_INTERNAL_ERROR);
            pTftpSession->enmTftpFmt = g_TftpTransferFmtDesc[idxFmt].enmType;
        }
        else if (fWithArg)
        {
            if (!RTStrICmp("blksize", g_TftpDesc[idxOptionArg].pszName))
                rc = RTStrToInt16Full(pszTftpRRQRaw, 0, (int16_t *)&pTftpSession->u16BlkSize);
            else if (!RTStrICmp("size", g_TftpDesc[idxOptionArg].pszName))
                rc = RTStrToInt16Full(pszTftpRRQRaw, 0, (int16_t *)&pTftpSession->u16Size);
            else if (!RTStrICmp("tsize", g_TftpDesc[idxOptionArg].pszName))
                rc = RTStrToInt16Full(pszTftpRRQRaw, 0, (int16_t *)&pTftpSession->u16TSize);
            else if (!RTStrICmp("timeoute", g_TftpDesc[idxOptionArg].pszName))
                rc = RTStrToInt16Full(pszTftpRRQRaw, 0, (int16_t *)&pTftpSession->u16Timeout);
            else
                rc = VERR_INVALID_PARAMETER;
            if (RT_FAILURE(rc))
            {
                LogFlowFuncLeaveRC(rc);
                AssertRCReturn(rc,rc);
            }
            fWithArg = 0;
            idxOptionArg = 0;
        }
        else
        {
            rc = tftpFindOptionIdxbyName(&idxOptionArg, pszTftpRRQRaw);
            if (RT_SUCCESS(rc))
                fWithArg = 1;
            else
            {
                LogFlowFuncLeaveRC(rc);
                AssertRCReturn(rc,rc);
            }
        }
        pszTftpRRQRaw += idxTftpRRQRaw;
        cbTftpRRQRaw -= idxTftpRRQRaw;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int tftpAllocateSession(PNATState pData, PCTFTPIPHDR pcTftpIpHeader)
{
    PTFTPSESSION pTftpSession;
    int rc = VINF_SUCCESS;
    int idxSession;

    for (idxSession = 0; idxSession < TFTP_SESSIONS_MAX; idxSession++)
    {
        pTftpSession = &pData->aTftpSessions[idxSession];

        if (!pTftpSession->fInUse)
            goto found;

        /* sessions time out after 5 inactive seconds */
        if ((int)(curtime - pTftpSession->iTimestamp) > 5000)
            goto found;
    }

    return -1;

 found:
    memset(pTftpSession, 0, sizeof(*pTftpSession));
    memcpy(&pTftpSession->IpClientAddress, &pcTftpIpHeader->IPv4Hdr.ip_src, sizeof(pTftpSession->IpClientAddress));
    pTftpSession->u16ClientPort = pcTftpIpHeader->UdpHdr.uh_sport;
    rc = tftpSessionOptionParse(pTftpSession, pcTftpIpHeader);
    AssertRCReturn(rc, -1);

    tftpSessionUpdate(pData, pTftpSession);

    return idxSession;
}

static int tftpSessionFind(PNATState pData, PCTFTPIPHDR pcTftpIpHeader)
{
    PTFTPSESSION pTftpSession;
    int k;

    for (k = 0; k < TFTP_SESSIONS_MAX; k++)
    {
        pTftpSession = &pData->aTftpSessions[k];

        if (pTftpSession->fInUse)
        {
            if (!memcmp(&pTftpSession->IpClientAddress, &pcTftpIpHeader->IPv4Hdr.ip_src, sizeof(pTftpSession->IpClientAddress)))
            {
                if (pTftpSession->u16ClientPort == pcTftpIpHeader->UdpHdr.uh_sport)
                    return k;
            }
        }
    }

    return -1;
}

DECLINLINE(int) pftpSessionOpenFile(PNATState pData, PTFTPSESSION pTftpSession, PRTFILE pSessionFile)
{
    char aszSessionFileName[TFTP_FILENAME_MAX];
    int cbSessionFileName;
    int rc = VINF_SUCCESS;
    cbSessionFileName = RTStrPrintf(aszSessionFileName, TFTP_FILENAME_MAX, "%s/%s",
                    tftp_prefix, pTftpSession->pszFilename);
    if (cbSessionFileName >= TFTP_FILENAME_MAX)
    {
        LogFlowFuncLeaveRC(VERR_INTERNAL_ERROR);
        return VERR_INTERNAL_ERROR;
    }

    if (!RTFileExists(aszSessionFileName))
    {
        LogFlowFuncLeaveRC(VERR_FILE_NOT_FOUND);
        return VERR_FILE_NOT_FOUND;
    }

    rc = RTFileOpen(pSessionFile, aszSessionFileName, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* @todo: rewrite this */
DECLINLINE(int) tftpSessionEvaluateBlkSize(PNATState pData, PTFTPSESSION pTftpSession)
{
    int rc = VINF_SUCCESS;
    RTFILE hSessionFile;
    uint64_t cbSessionFile = 0;
    int      idxRFC2348TftpSessionBlkSize = 0;
    uint32_t cBlockSessionFile = 0;
    LogFlowFunc(("pTftpSession:%p\n", pTftpSession));

    rc = pftpSessionOpenFile(pData, pTftpSession, &hSessionFile);
    if (RT_FAILURE(rc))
    {
        LogFlowFuncLeave();
        return rc;
    }

    rc = RTFileGetSize(hSessionFile, &cbSessionFile);
    RTFileClose(hSessionFile);
    if (RT_FAILURE(rc))
    {
        LogFlowFuncLeave();
        return rc;
    }

    if (!pTftpSession->u16BlkSize)
    {
        pTftpSession->u16BlkSize = 1428;
    }
    cBlockSessionFile = ASMDivU64ByU32RetU32(cbSessionFile, pTftpSession->u16BlkSize);
    while (   cBlockSessionFile >= UINT16_MAX
           && idxRFC2348TftpSessionBlkSize <= RT_ELEMENTS(g_au16RFC2348TftpSessionBlkSize))
    {
        if (pTftpSession->u16BlkSize > g_au16RFC2348TftpSessionBlkSize[idxRFC2348TftpSessionBlkSize])
        {
            idxRFC2348TftpSessionBlkSize++;
            continue;
        }


        idxRFC2348TftpSessionBlkSize++;
        /* No bigger values in RFC2348 */
        AssertReturn(idxRFC2348TftpSessionBlkSize <= RT_ELEMENTS(g_au16RFC2348TftpSessionBlkSize), VERR_INTERNAL_ERROR);
        if (g_au16RFC2348TftpSessionBlkSize[idxRFC2348TftpSessionBlkSize] >= if_maxlinkhdr)
        {
            /* Buffer size is too large for current settings */
            rc = VERR_BUFFER_OVERFLOW;
            LogFlowFuncLeaveRC(rc);
        }
    }
    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLINLINE(int) tftpSend(PNATState pData,
                         PTFTPSESSION pTftpSession,
                         struct mbuf *pMBuf,
                         PCTFTPIPHDR pcTftpIpHeaderRecv)
{
    int rc = VINF_SUCCESS;
    struct sockaddr_in saddr, daddr;
    LogFlowFunc(("pMBuf:%p, pcTftpIpHeaderRecv:%p\n", pMBuf, pcTftpIpHeaderRecv));
    saddr.sin_addr = pcTftpIpHeaderRecv->IPv4Hdr.ip_dst;
    saddr.sin_port = pcTftpIpHeaderRecv->UdpHdr.uh_dport;

    daddr.sin_addr = pTftpSession->IpClientAddress;
    daddr.sin_port = pTftpSession->u16ClientPort;


    pMBuf->m_data += sizeof(struct udpiphdr);
    pMBuf->m_len -= sizeof(struct udpiphdr);
    udp_output2(pData, NULL, pMBuf, &saddr, &daddr, IPTOS_LOWDELAY);
    LogFlowFuncLeaveRC(rc);
    return rc;
}
DECLINLINE(int) tftpSendError(PNATState pData, PTFTPSESSION pTftpSession, uint16_t errorcode, const char *msg, PCTFTPIPHDR pcTftpIpHeaderRecv);

DECLINLINE(int) tftpReadDataBlock(PNATState pData,
                                  PTFTPSESSION pTftpSession,
                                  uint16_t u16BlockNr,
                                  uint8_t *pu8Data,
                                  int *pcbReadData)
{
    RTFILE  hSessionFile;
    int rc = VINF_SUCCESS;
    LogFlowFunc(("pTftpSession:%p, u16BlockNr:%RX16, pu8Data:%p, pcbReadData:%p\n",
                    pTftpSession,
                    u16BlockNr,
                    pu8Data,
                    pcbReadData));

    rc = pftpSessionOpenFile(pData, pTftpSession, &hSessionFile);
    if (RT_FAILURE(rc))
    {
        LogFlowFuncLeaveRC(rc);
        return rc;
    }

    if (pcbReadData)
    {
        rc = RTFileSeek(hSessionFile,
                        u16BlockNr * pTftpSession->u16BlkSize,
                        RTFILE_SEEK_BEGIN,
                        NULL);
        if (RT_FAILURE(rc))
        {
            RTFileClose(hSessionFile);
            LogFlowFuncLeaveRC(rc);
            return rc;
        }
        rc = RTFileRead(hSessionFile, pu8Data, pTftpSession->u16BlkSize, (size_t *)pcbReadData);
        if (RT_FAILURE(rc))
        {
            RTFileClose(hSessionFile);
            LogFlowFuncLeaveRC(rc);
            return rc;
        }
    }

    rc = RTFileClose(hSessionFile);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLINLINE(int) tftpAddOptionToOACK(PNATState pData, struct mbuf *pMBuf, const char *pszOptName, uint16_t u16OptValue)
{
    char aszOptionBuffer[256];
    int iOptLength = 0;
    int rc = VINF_SUCCESS;
    int cbMBufCurrent = pMBuf->m_len;
    LogFlowFunc(("pMBuf:%p, pszOptName:%s, u16OptValue:%u\n", pMBuf, pszOptName, u16OptValue));
    AssertPtrReturn(pMBuf, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszOptName, VERR_INVALID_PARAMETER);

    RT_ZERO(aszOptionBuffer);
    iOptLength += RTStrPrintf(aszOptionBuffer, 256 , "%s", pszOptName) + 1;
    iOptLength += RTStrPrintf(aszOptionBuffer + iOptLength, 256 - iOptLength , "%u", u16OptValue) + 1;
    if (iOptLength > M_TRAILINGSPACE(pMBuf))
        rc = VERR_BUFFER_OVERFLOW; /* buffer too small */
    else
    {
        pMBuf->m_len += iOptLength;
        m_copyback(pData, pMBuf, cbMBufCurrent, iOptLength, aszOptionBuffer);
    }
    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLINLINE(int) tftpSendOACK(PNATState pData,
                          PTFTPSESSION pTftpSession,
                          PCTFTPIPHDR pcTftpIpHeaderRecv)
{
    struct mbuf *m;
    PTFTPIPHDR pTftpIpHeader;
    int rc = VINF_SUCCESS;

    rc = tftpSessionEvaluateBlkSize(pData, pTftpSession);
    if (RT_FAILURE(rc))
    {
        tftpSendError(pData, pTftpSession, 2, "Internal Error (blksize evaluation)", pcTftpIpHeaderRecv);
        LogFlowFuncLeave();
        return -1;
    }

    m = slirpTftpMbufAlloc(pData);
    if (!m)
        return -1;



    m->m_data += if_maxlinkhdr;
    m->m_pkthdr.header = mtod(m, void *);
    pTftpIpHeader = mtod(m, PTFTPIPHDR);
    m->m_len = sizeof(TFTPIPHDR) - sizeof(uint16_t); /* no u16TftpOpCode */

    pTftpIpHeader->u16TftpOpType = RT_H2N_U16_C(TFTP_OACK);

    if (pTftpSession->u16BlkSize)
        rc = tftpAddOptionToOACK(pData, m, "blksize", pTftpSession->u16BlkSize);
    if (   RT_SUCCESS(rc)
        && pTftpSession->u16Size)
        rc = tftpAddOptionToOACK(pData, m, "size", pTftpSession->u16Size);
    if (   RT_SUCCESS(rc)
        && pTftpSession->u16TSize)
        rc = tftpAddOptionToOACK(pData, m, "tsize", pTftpSession->u16TSize);

    rc = tftpSend(pData, pTftpSession, m, pcTftpIpHeaderRecv);
    return RT_SUCCESS(rc) ? 0 : -1;
}

DECLINLINE(int) tftpSendError(PNATState pData,
                              PTFTPSESSION pTftpSession,
                              uint16_t errorcode,
                              const char *msg,
                              PCTFTPIPHDR pcTftpIpHeaderRecv)
{
    struct mbuf *m = NULL;
    PTFTPIPHDR pTftpIpHeader = NULL;

    m = slirpTftpMbufAlloc(pData);
    if (!m)
        return -1;

    m->m_data += if_maxlinkhdr;
    m->m_len = sizeof(TFTPIPHDR)
             + strlen(msg) + 1; /* ending zero */
    m->m_pkthdr.header = mtod(m, void *);
    pTftpIpHeader = mtod(m, PTFTPIPHDR);

    pTftpIpHeader->u16TftpOpType = RT_H2N_U16_C(TFTP_ERROR);
    pTftpIpHeader->Core.u16TftpOpCode = RT_H2N_U16(errorcode);

    m_copyback(pData, m, sizeof(TFTPIPHDR), strlen(msg) + 1 /* copy ending zerro*/, (c_caddr_t)msg);

    tftpSend(pData, pTftpSession, m, pcTftpIpHeaderRecv);

    tftpSessionTerminate(pTftpSession);

    return 0;
}

static int tftpSendData(PNATState pData,
                          PTFTPSESSION pTftpSession,
                          u_int16_t block_nr,
                          PCTFTPIPHDR pcTftpIpHeaderRecv)
{
    struct mbuf *m;
    PTFTPIPHDR pTftpIpHeader;
    int nobytes;
    int rc = VINF_SUCCESS;

    /* we should be sure that we don't talk about file offset prior 0 ;) */
    if (block_nr < 1)
        return -1;

    m = slirpTftpMbufAlloc(pData);
    if (!m)
        return -1;

    m->m_data += if_maxlinkhdr;
    m->m_pkthdr.header = mtod(m, void *);
    pTftpIpHeader = mtod(m, PTFTPIPHDR);
    m->m_len = sizeof(TFTPIPHDR);

    pTftpIpHeader->u16TftpOpType = RT_H2N_U16_C(TFTP_DATA);
    pTftpIpHeader->Core.u16TftpOpCode = RT_H2N_U16(block_nr);

    rc = tftpReadDataBlock(pData, pTftpSession, block_nr - 1, (uint8_t *)&pTftpIpHeader->Core.u16TftpOpCode + sizeof(uint16_t), &nobytes);

    if (RT_SUCCESS(rc))
    {
        m->m_len += nobytes;
        tftpSend(pData, pTftpSession, m, pcTftpIpHeaderRecv);
        if (nobytes > 0)
            tftpSessionUpdate(pData, pTftpSession);
        else
            tftpSessionTerminate(pTftpSession);
    }
    else
    {
        m_freem(pData, m);
        tftpSendError(pData, pTftpSession, 1, "File not found", pcTftpIpHeaderRecv);
        /* send "file not found" error back */
        return -1;
    }

    return 0;
}

DECLINLINE(void) tftpProcessRRQ(PNATState pData, PCTFTPIPHDR pTftpIpHeader, int pktlen)
{
    PTFTPSESSION pTftpSession;
    int idxTftpSession = 0;
    uint8_t *pu8Payload = NULL;
    int     cbPayload = 0;
    int cbFileName = 0;

    AssertPtrReturnVoid(pTftpIpHeader);
    AssertPtrReturnVoid(pData);
    AssertReturnVoid(pktlen > sizeof(TFTPIPHDR));
    LogFlowFunc(("ENTER: pTftpIpHeader:%p, pktlen:%d\n", pTftpIpHeader, pktlen));

    idxTftpSession = tftpAllocateSession(pData, pTftpIpHeader);
    if (idxTftpSession < 0)
    {
        LogFlowFuncLeave();
        return;
    }

    pTftpSession = &pData->aTftpSessions[idxTftpSession];
    pu8Payload = (uint8_t *)&pTftpIpHeader->Core;
    cbPayload = pktlen - sizeof(TFTPIPHDR);

    cbFileName = RTStrNLen((char *)pu8Payload, cbPayload);
    /* We assume that file name should finish with '\0' and shouldn't bigger
     *  than buffer for name storage.
     */
    AssertReturnVoid(   cbFileName < cbPayload
                     && cbFileName < TFTP_FILENAME_MAX /* current limit in tftp session handle */
                     && cbFileName);

    /* Dont't bother with rest processing in case of invalid access */
    if (RT_FAILURE(tftpSecurityFilenameCheck(pData, pTftpSession)))
    {
        tftpSendError(pData, pTftpSession, 2, "Access violation", pTftpIpHeader);
        LogFlowFuncLeave();
        return;
    }



    if (RT_UNLIKELY(!tftpIsSupportedTransferMode(pTftpSession)))
    {
        tftpSendError(pData, pTftpSession, 4, "Unsupported transfer mode", pTftpIpHeader);
        LogFlowFuncLeave();
        return;
    }


    tftpSendOACK(pData, pTftpSession, pTftpIpHeader);
    LogFlowFuncLeave();
    return;
}

static void tftpProcessACK(PNATState pData, PTFTPIPHDR pTftpIpHeader)
{
    int s;

    s = tftpSessionFind(pData, pTftpIpHeader);
    if (s < 0)
        return;

    if (tftpSendData(pData, &pData->aTftpSessions[s],
                       RT_N2H_U16(pTftpIpHeader->Core.u16TftpOpCode) + 1, pTftpIpHeader) < 0)
    {
        /* XXX */
    }
}

DECLCALLBACK(void) tftp_input(PNATState pData, struct mbuf *pMbuf)
{
    PTFTPIPHDR pTftpIpHeader = NULL;
    AssertPtr(pData);
    AssertPtr(pMbuf);
    pTftpIpHeader = mtod(pMbuf, PTFTPIPHDR);

    switch(RT_N2H_U16(pTftpIpHeader->u16TftpOpType))
    {
        case TFTP_RRQ:
            tftpProcessRRQ(pData, pTftpIpHeader, m_length(pMbuf, NULL));
            break;

        case TFTP_ACK:
            tftpProcessACK(pData, pTftpIpHeader);
            break;
        default:
            LogFlowFuncLeave();
            return;
    }
}
