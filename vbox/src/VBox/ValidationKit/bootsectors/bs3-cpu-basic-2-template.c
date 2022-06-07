/* $Id: bs3-cpu-basic-2-template.c 60024 2016-03-15 08:59:49Z vboxsync $ */
/** @file
 * BS3Kit - bs3-cpu-basic-2, C code template.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
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
 */


#ifdef BS3_INSTANTIATING_MODE

extern BS3_DECL(void) TMPL_NM(bs3CpuBasic2_TssGateEsp_IntXx)(void);

BS3_DECL(uint8_t) TMPL_NM(bs3CpuBasic2_TssGateEsp)(uint8_t bMode)
{
    uint8_t         bRet = 0;
# if TMPL_MODE == BS3_MODE_PE16 \
 || TMPL_MODE == BS3_MODE_PE16_32
    BS3TRAPFRAME    TrapCtx;
    BS3REGCTX       Ctx;

    Bs3RegCtxSave(&Ctx);
    Ctx.rip.u = (uintptr_t)&TMPL_NM(bs3CpuBasic2_TssGateEsp_IntXx);

    /*
     * Check that the stuff works first.
     */
     if (Bs3TrapSetJmp(&TrapCtx))
     {

         Bs3TrapUnsetJmp();
     }
     else
     {
         /* trapped. */
     }


# else
    bRet = BS3TESTDOMODE_SKIPPED;
# endif

    /*
     * Re-initialize the IDT.
     */
#  if BS3_MODE_IS_16BIT_SYS(TMPL_MODE)
    Bs3Trap16Init();
#  elif BS3_MODE_IS_32BIT_SYS(TMPL_MODE)
    Bs3Trap32Init();
#  elif BS3_MODE_IS_32BIT_SYS(TMPL_MODE)
    Bs3Trap64Init();
#  endif

    return bRet;
}


#endif /* BS3_INSTANTIATING_MODE */
