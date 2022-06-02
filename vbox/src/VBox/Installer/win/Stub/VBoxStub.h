/* $Id: VBoxStub.h 31659 2010-08-13 15:13:08Z vboxsync $ */
/** @file
 * VBoxStub - VirtualBox's Windows installer stub.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#pragma once

#define VBOX_STUB_TITLE "VirtualBox Installer"

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;

