/* $Id: int-1.d 40923 2012-04-14 15:28:25Z vboxsync $ */
/** @file
 * DTracing VBox - Interrupt Experiment #1.
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

uint64_t        g_aStarts[uint32_t];
unsigned int    g_cHits;

vboxvmm*:::pdm-irq-high
/* /args[1] != 0/ */
{
    printf("high: tag=%#x %llx -> %llx", args[1], g_aStarts[args[1]], timestamp);
    g_aStarts[args[1]] = timestamp;
}

vboxvmm*:::pdm-irq-get
/g_aStarts[args[1]] > 0/
/* /args[1] != 0/ */
{
    @interrupts[args[3]] = count();
    printf("get:  tag=%#x %llx - %llx = %llx", args[1], timestamp, g_aStarts[args[1]], timestamp - g_aStarts[args[1]]);
    @dispavg[args[3]]  = avg(timestamp - g_aStarts[args[1]]);
    @dispmax[args[3]]  = max(timestamp - g_aStarts[args[1]]);
    @dispmin[args[3]]  = min(timestamp - g_aStarts[args[1]]);
    g_aStarts[args[1]] = 0;
    g_cHits++;
}

vboxvmm*:::pdm-irq-get
/g_cHits >= 512/
{
    exit(0);
}


END
{
    printf("\nInterrupt distribution:");
    printa(@interrupts);
    printf("Average dispatch latency:");
    printa(@dispavg);
    printf("Minimum dispatch latency:");
    printa(@dispavg);
    printf("Maximum dispatch latency:");
    printa(@dispmax);
}
