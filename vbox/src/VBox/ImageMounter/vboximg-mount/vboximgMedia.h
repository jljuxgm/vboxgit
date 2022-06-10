/* $Id: vboximgMedia.h 76574 2019-01-01 05:59:26Z vboxsync $ $Revision: 76574 $ $Date: 2019-01-01 13:59:26 +0800 (Tue, 01 Jan 2019) $ $Author: vboxsync $ */

/** @file
 * vboximgMedia.h
 */

/*
 * Copyright (C) 2008-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_vboximg_mount_vboximgMedia_h
#define VBOX_INCLUDED_SRC_vboximg_mount_vboximgMedia_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

typedef struct MEDIUMINFO
{
    char *name;
    char *uuid;
    char *location;
    char *description;
    char *state;
    char *size;
    char *format;
    int ro;
} MEDIUMINFO;

int vboximgListVMs(IVirtualBox *pVirtualBox);
char *vboximgScaledSize(size_t size);

#endif /* ____H_VBOXIMGMEDIA */
