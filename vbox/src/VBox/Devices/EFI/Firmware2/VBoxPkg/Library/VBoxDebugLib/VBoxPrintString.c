/* $Id: VBoxPrintString.c 29081 2010-05-05 13:32:04Z vboxsync $ */
/** @file
 * VBoxPrintString.c - Implementation of the VBoxPrintString() debug logging routine.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxDebugLib.h"
#include "DevEFI.h"


/**
 * Prints a string to the EFI debug port.
 *
 * @returns The string length.
 * @param   pszString       The string to print.
 */
size_t VBoxPrintString(const char *pszString)
{
    const char *pszEnd = pszString;
    while (*pszEnd)
        pszEnd++;
    ASMOutStrU8(EFI_DEBUG_PORT, (uint8_t const *)pszString, pszEnd - pszString);
    return pszEnd - pszString;
}

