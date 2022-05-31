/* $Id: VBoxNetFltPt-win.h 21333 2009-07-07 14:39:32Z vboxsync $ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Windows Specific Code. Protocol edge of ndis filter driver
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */
/*
 * Based in part on Microsoft DDK sample code for Ndis Intermediate Miniport passthru driver sample.
 * Copyright (c) 1993-1999, Microsoft Corporation
 */

#ifndef ___VBoxNetFltPt_win_h___
#define ___VBoxNetFltPt_win_h___

#ifdef VBOXNETADP
# error "No protocol edge"
#endif
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtRegister(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtDeregister();
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtDoUnbinding(PADAPT pAdapt, bool bOnUnbind);
DECLHIDDEN(VOID) vboxNetFltWinPtFlushReceiveQueue(IN PADAPT pAdapt, IN bool bReturn);
DECLHIDDEN(VOID) vboxNetFltWinPtRequestComplete(IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_REQUEST NdisRequest, IN NDIS_STATUS Status);
DECLHIDDEN(bool) vboxNetFltWinPtCloseAdapter(PADAPT pAdapt, PNDIS_STATUS pStatus);

#ifdef VBOX_NETFLT_ONDEMAND_BIND
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtDoBinding(IN PADAPT pAdapt);
#else
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtDoBinding(IN PADAPT pAdapt, IN PNDIS_STRING pOurDeviceName, IN PNDIS_STRING pBindToDeviceName);
DECLHIDDEN(NDIS_HANDLE) vboxNetFltWinPtGetHandle();
#endif

#endif
