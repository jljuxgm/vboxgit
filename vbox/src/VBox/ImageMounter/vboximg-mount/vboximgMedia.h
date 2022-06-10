/* $Id: vboximgMedia.h 76271 2018-12-18 05:29:30Z vboxsync $ $Revision: 76271 $ $Date: 2018-12-18 13:29:30 +0800 (Tue, 18 Dec 2018) $ $Author: vboxsync $ */

/** @file
 * vboximgMedia.h
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VBOXIMGMEDIA
#define ____H_VBOXIMGMEDIA

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
