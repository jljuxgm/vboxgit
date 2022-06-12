/* $Id: AudioTestServiceInternal.h 89182 2021-05-19 15:59:03Z vboxsync $ */
/** @file
 * AudioTestService - Audio test execution server, Internal Header.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Audio_AudioTestServiceInternal_h
#define VBOX_INCLUDED_SRC_Audio_AudioTestServiceInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/getopt.h>
#include <iprt/stream.h>

#include "AudioTestServiceProtocol.h"

RT_C_DECLS_BEGIN

/** Opaque ATS transport layer specific client data. */
typedef struct ATSTRANSPORTCLIENT *PATSTRANSPORTCLIENT;
typedef PATSTRANSPORTCLIENT *PPATSTRANSPORTCLIENT;

/**
 * Transport layer descriptor.
 */
typedef struct ATSTRANSPORT
{
    /** The name. */
    char            szName[16];
    /** The description. */
    const char     *pszDesc;
    /** Pointer to an array of options. */
    PCRTGETOPTDEF   paOpts;
    /** The number of options in the array. */
    size_t          cOpts;

    /**
     * Print the usage information for this transport layer.
     *
     * @param   pStream             The stream to print the usage info to.
     *
     * @remarks This is only required if TXSTRANSPORT::cOpts is greater than 0.
     */
    DECLR3CALLBACKMEMBER(void, pfnUsage, (PRTSTREAM pStream));

    /**
     * Handle an option.
     *
     * When encountering an options that is not part of the base options, we'll call
     * this method for each transport layer until one handles it.
     *
     * @retval  VINF_SUCCESS if handled.
     * @retval  VERR_TRY_AGAIN if not handled.
     * @retval  VERR_INVALID_PARAMETER if we should exit with a non-zero status.
     *
     * @param   ch                  The short option value.
     * @param   pVal                Pointer to the value union.
     *
     * @remarks This is only required if TXSTRANSPORT::cOpts is greater than 0.
     */
    DECLR3CALLBACKMEMBER(int, pfnOption, (int ch, PCRTGETOPTUNION pVal));

    /**
     * Initializes the transport layer.
     *
     * @returns IPRT status code.  On errors, the transport layer shall call
     *          RTMsgError to display the error details to the user.
     */
    DECLR3CALLBACKMEMBER(int, pfnInit, (void));

    /**
     * Terminate the transport layer, closing and freeing resources.
     *
     * On errors, the transport layer shall call RTMsgError to display the error
     * details to the user.
     */
    DECLR3CALLBACKMEMBER(void, pfnTerm, (void));

    /**
     * Waits for a new client to connect and returns the client specific data on
     * success.
     */
    DECLR3CALLBACKMEMBER(int, pfnWaitForConnect, (PPATSTRANSPORTCLIENT ppClientNew));

    /**
     * Polls for incoming packets.
     *
     * @returns true if there are pending packets, false if there isn't.
     * @param   pClient             The client to poll for data.
     */
    DECLR3CALLBACKMEMBER(bool, pfnPollIn, (PATSTRANSPORTCLIENT pClient));

    /**
     * Adds any pollable handles to the poll set.
     *
     * @returns IPRT status code.
     * @param   hPollSet            The poll set to add them to.
     * @param   pClient             The transport client structure.
     * @param   idStart             The handle ID to start at.
     */
    DECLR3CALLBACKMEMBER(int, pfnPollSetAdd, (RTPOLLSET hPollSet, PATSTRANSPORTCLIENT pClient, uint32_t idStart));

    /**
     * Removes the given client frmo the given pollset.
     *
     * @returns IPRT status code.
     * @param   hPollSet            The poll set to remove from.
     * @param   pClient             The transport client structure.
     * @param   idStart             The handle ID to remove.
     */
    DECLR3CALLBACKMEMBER(int, pfnPollSetRemove, (RTPOLLSET hPollSet, PATSTRANSPORTCLIENT pClient, uint32_t idStart));

    /**
     * Receives an incoming packet.
     *
     * This will block until the data becomes available or we're interrupted by a
     * signal or something.
     *
     * @returns IPRT status code.  On error conditions other than VERR_INTERRUPTED,
     *          the current operation will be aborted when applicable.  When
     *          interrupted, the transport layer will store the data until the next
     *          receive call.
     *
     * @param   pClient             The transport client structure.
     * @param   ppPktHdr            Where to return the pointer to the packet we've
     *                              read.  This is allocated from the heap using
     *                              RTMemAlloc (w/ ATSPKT_ALIGNMENT) and must be
     *                              free by calling RTMemFree.
     */
    DECLR3CALLBACKMEMBER(int, pfnRecvPkt, (PATSTRANSPORTCLIENT pClient, PPATSPKTHDR ppPktHdr));

    /**
     * Sends an outgoing packet.
     *
     * This will block until the data has been written.
     *
     * @returns IPRT status code.
     * @retval  VERR_INTERRUPTED if interrupted before anything was sent.
     *
     * @param   pClient             The transport client structure.
     * @param   pPktHdr             The packet to send.  The size is given by
     *                              aligning the size in the header by
     *                              ATSPKT_ALIGNMENT.
     */
    DECLR3CALLBACKMEMBER(int, pfnSendPkt, (PATSTRANSPORTCLIENT pClient, PCATSPKTHDR pPktHdr));

    /**
     * Sends a babble packet and disconnects the client (if applicable).
     *
     * @param   pClient             The transport client structure.
     * @param   pPktHdr             The packet to send.  The size is given by
     *                              aligning the size in the header by
     *                              ATSPKT_ALIGNMENT.
     * @param   cMsSendTimeout      The send timeout measured in milliseconds.
     */
    DECLR3CALLBACKMEMBER(void, pfnBabble, (PATSTRANSPORTCLIENT pClient, PCATSPKTHDR pPktHdr, RTMSINTERVAL cMsSendTimeout));

    /**
     * Notification about a client HOWDY.
     *
     * @param   pClient             The transport client structure.
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyHowdy, (PATSTRANSPORTCLIENT pClient));

    /**
     * Notification about a client BYE.
     *
     * For connection oriented transport layers, it would be good to disconnect the
     * client at this point.
     *
     * @param   pClient             The transport client structure.
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyBye, (PATSTRANSPORTCLIENT pClient));

    /**
     * Notification about a REBOOT or SHUTDOWN.
     *
     * For connection oriented transport layers, stop listening for and
     * accepting at this point.
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyReboot, (void));

    /** Non-zero end marker. */
    uint32_t u32EndMarker;
} ATSTRANSPORT;
/** Pointer to a const transport layer descriptor. */
typedef const struct ATSTRANSPORT *PCATSTRANSPORT;


extern ATSTRANSPORT const g_TcpTransport;

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioTestServiceInternal_h */
