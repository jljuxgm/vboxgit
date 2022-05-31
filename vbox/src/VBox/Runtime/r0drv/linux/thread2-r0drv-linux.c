/* $Id: thread2-r0drv-linux.c 19912 2009-05-22 14:00:33Z vboxsync $ */
/** @file
 * IPRT - Threads (Part 2), Ring-0 Driver, Linux.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-linux-kernel.h"

#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/err.h>
#include "internal/thread.h"


/** @todo Later.
RTDECL(RTTHREAD) RTThreadSelf(void)
{
    return rtThreadGetByNative((RTNATIVETHREAD)current);
}
*/


RTDECL(bool) RTThreadPreemptIsEnabled(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);
    return !in_atomic() && !irqs_disabled();
}


RTDECL(bool) RTThreadPreemptIsPending(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 4)
    return test_tsk_thread_flag(current, TIF_NEED_RESCHED);

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 20)
    return need_resched();

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 1, 110)
    return current->need_resched != 0;

#else
    return need_resched != 0;
#endif
}

