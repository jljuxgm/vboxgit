/* $Id: VBoxMPHGSMI.h 76540 2018-12-30 06:26:37Z vboxsync $ */
/** @file
 * VBox Miniport HGSMI related header
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOXMPHGSMI_H
#define VBOXMPHGSMI_H
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VBoxMPDevExt.h"

RT_C_DECLS_BEGIN
void VBoxSetupDisplaysHGSMI(PVBOXMP_COMMON pCommon, PHYSICAL_ADDRESS phVRAM, uint32_t ulApertureSize, uint32_t cbVRAM, uint32_t fCaps);
void VBoxFreeDisplaysHGSMI(PVBOXMP_COMMON pCommon);
RT_C_DECLS_END

#endif /*!VBOXMPHGSMI_H*/
