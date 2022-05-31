; $Id: SrvIntNetR0A-win.asm 21342 2009-07-07 15:29:35Z vboxsync $ */
;; @file
; Internal networking - The ring 0 service.
;

;
; Copyright (C) 2006-2008 Sun Microsystems, Inc.
;
; Sun Microsystems, Inc. confidential
; All rights reserved
;

%define IN_DYNAMICLOAD_CODE

%include "iprt/ntwrap.mac"

NtWrapDrv2DynFunctionWithAllRegParams intnetNtWrap, intnetR0TrunkIfPortSetSGPhys
NtWrapDrv2DynFunctionWithAllRegParams intnetNtWrap, intnetR0TrunkIfPortRecv
NtWrapDrv2DynFunctionWithAllRegParams intnetNtWrap, intnetR0TrunkIfPortSGRetain
NtWrapDrv2DynFunctionWithAllRegParams intnetNtWrap, intnetR0TrunkIfPortSGRelease

