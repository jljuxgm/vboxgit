/* $Id: tstVBoxGuest-solaris.cpp 40311 2012-03-01 12:09:56Z vboxsync $ */
/** @file
 *
 * Test cases for VBoxGuest-solaris.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/test.h>

int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstVBoxGuest-solaris", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

