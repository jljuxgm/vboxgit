/* $Id: VBoxCPP.cpp 41179 2012-05-06 20:31:02Z vboxsync $ */
/** @file
 * VBox Build Tool - A mini C Preprocessor. 
 *  
 * This is not attempting to be standard compliant, just get the job done! 
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/VBoxTpG.h>

#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "scmstream.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The bitmap type. */
#define VBCPP_BITMAP_TYPE                   uint64_t
/** The bitmap size as a multiple of VBCPP_BITMAP_TYPE. */
#define VBCPP_BITMAP_SIZE                   (128 / 64)
/** Checks if a bit is set. */
#define VBCPP_BITMAP_IS_SET(a_bm, a_ch)     ASMBitTest(a_bm, (a_ch) & 0x7f)
/** Sets a bit. */
#define VBCPP_BITMAP_SET(a_bm, a_ch)        ASMBitSet(a_bm, (a_ch) & 0x7f)
/** Empties the bitmap. */
#define VBCPP_BITMAP_EMPTY(a_bm)            do { (a_bm)[0] = 0; (a_bm)[1] = 0; } while (0)
/** Joins to bitmaps by OR'ing their values.. */
#define VBCPP_BITMAP_OR(a_bm1, a_bm2)       do { (a_bm1)[0] |= (a_bm2)[0]; (a_bm1)[1] |= (a_bm2)[1]; } while (0)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The preprocessor mode.
 */
typedef enum VBCPPMODE
{
    kVBCppMode_Invalid = 0,
/*    kVBCppMode_Full,*/
    kVBCppMode_Selective,
    kVBCppMode_SelectiveD,
    kVBCppMode_End
} VBCPPMODE;


/** 
 * A define.
 */
typedef struct VBCPPDEF
{
    /** The string space core. */
    RTSTRSPACECORE      Core;
    /** Whether it's a function. */
    bool                fFunction;
    /** Variable argument count. */
    bool                fVarArg;
    /** The number of known arguments.*/
    uint32_t            cArgs;
    /** Pointer to a list of argument names. */
    const char        **papszArgs;
    /** Lead character bitmap for the argument names. */
    VBCPP_BITMAP_TYPE   bmArgs[VBCPP_BITMAP_SIZE];
    /** The define value.  (This is followed by the name and arguments.) */
    char                szValue[1];
} VBCPPDEF;
/** Pointer to a define. */
typedef VBCPPDEF *PVBCPPDEF;


/**
 * Expansion context.
 */
typedef struct VBCPPCTX
{
    /** The next context on the stack. */
    struct VBCPPCTX    *pUp;
    /** The define being expanded. */
    PVBCPPDEF           pDef;
    /** Arguments. */
    struct VBCPPCTXARG
    {
        /** The value. */
        const char *pchValue;
        /** The value length. */
        const char *cchValue;
    }                   aArgs[1];
} VBCPPCTX;
/** Pointer to an define expansion context. */
typedef VBCPPCTX *PVBCPPCTX;


/**
 * C Preprocessor instance data.
 */
typedef struct VBCPP
{
    /** @name Options
     * @{ */ 
    /** The preprocessing mode. */
    VBCPPMODE           enmMode;
    /** Whether to keep comments. */
    bool                fKeepComments;

    /** The number of include directories. */
    uint32_t            cIncludes;
    /** Array of directories to search for include files. */
    char              **papszIncludes;

    /** The name of the input file. */
    const char         *pszInput;
    /** The name of the output file. NULL if stdout. */
    const char         *pszOutput;
    /** @} */
    
    /** The define string space. */
    RTSTRSPACE          StrSpace;
    /** Indicates whether a C-word might need expansion.
     * The bitmap is indexed by C-word lead character.  Bits that are set 
     * indicates that the lead character is used in a \#define that we know and 
     * should expand. */ 
    VBCPP_BITMAP_TYPE   bmDefined[VBCPP_BITMAP_SIZE];
    /** Indicates whether a C-word might need argument expansion.
     * The bitmap is indexed by C-word lead character.  Bits that are set 
     * indicates that the lead character is used in an argument of an currently 
     * expanding  \#define. */
    VBCPP_BITMAP_TYPE   bmArgs[VBCPP_BITMAP_SIZE];

    /** Expansion context stack. */
    PVBCPPCTX           pStack;
    /** The current stack depth. */
    uint32_t            cStackDepth;
    /** Whether the current line could be a preprocessor line.
     * This is set when EOL is encountered and cleared again when a 
     * non-comment-or-space character is encountered.  See vbcppPreprocess. */ 
    bool                fMaybePreprocessorLine;

    /** The current input stream. */
    PSCMSTREAM          pCurStrmInput;
    /** The input stream. */
    SCMSTREAM           StrmInput;
    /** The output stream. */
    SCMSTREAM           StrmOutput;

    /** The status of the whole job, as far as we know. */
    RTEXITCODE          rcExit;
    /** Whether StrmOutput is valid (for vbcppTerm). */
    bool                fStrmOutputValid;
} VBCPP;
/** Pointer to the C preprocessor instance data. */
typedef VBCPP *PVBCPP;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/



static void vbcppInit(PVBCPP pThis)
{
    pThis->enmMode          = kVBCppMode_Selective;
    pThis->fKeepComments    = true;
    pThis->cIncludes        = 0;
    pThis->cIncludes        = 0;
    pThis->papszIncludes    = NULL;
    pThis->pszInput         = NULL;
    pThis->pszOutput        = NULL;
    pThis->StrSpace         = NULL;
    pThis->pStack           = NULL;
    pThis->cStackDepth      = 0;
    VBCPP_BITMAP_EMPTY(pThis->bmDefined);
    VBCPP_BITMAP_EMPTY(pThis->bmArgs);
    RT_ZERO(pThis->StrmInput);
    RT_ZERO(pThis->StrmOutput);
    pThis->rcExit           = RTEXITCODE_SUCCESS;
    pThis->fStrmOutputValid = false;
}


/**
 * Displays an error message. 
 *  
 * @returns RTEXITCODE_FAILURE
 * @param   pThis               The C preprocessor instance.
 * @param   pszMsg              The message.
 * @param   ...                 Message arguments.
 */
static RTEXITCODE vbcppError(PVBCPP pThis, const char *pszMsg, ...)
{
    NOREF(pThis);
    va_list va;
    va_start(va, pszMsg);
    RTMsgErrorV(pszMsg, va);
    va_end(va);
    return pThis->rcExit = RTEXITCODE_FAILURE;
}


/**
 * Displays an error message. 
 *  
 * @returns RTEXITCODE_FAILURE
 * @param   pThis               The C preprocessor instance. 
 * @param   pszPos              Pointer to the offending character.
 * @param   pszMsg              The message.
 * @param   ...                 Message arguments.
 */
static RTEXITCODE vbcppErrorPos(PVBCPP pThis, const char *pszPos, const char *pszMsg, ...)
{
    NOREF(pszPos); NOREF(pThis);
    va_list va;
    va_start(va, pszMsg);
    RTMsgErrorV(pszMsg, va);
    va_end(va);
    return pThis->rcExit = RTEXITCODE_FAILURE;
}


/**
 * Checks if the given character is a valid C identifier lead character. 
 *  
 * @returns true / false.
 * @param   ch                  The character to inspect.
 */
DECLINLINE(bool) vbcppIsCIdentifierLeadChar(char ch)
{
    return RT_C_IS_ALPHA(ch)
        || ch == '_';
}


/**
 * Checks if the given character is a valid C identifier character. 
 *  
 * @returns true / false.
 * @param   ch                  The character to inspect.
 */
DECLINLINE(bool) vbcppIsCIdentifierChar(char ch)
{
    return RT_C_IS_ALNUM(ch)
        || ch == '_';
}



/**
 * 
 * @returns @c true if valid, @c false if not. Error message already displayed 
 *          on failure.
 * @param   pThis           The C preprocessor instance.
 * @param   pchIdentifier   The start of the identifier to validate.
 * @param   cchIdentifier   The length of the identifier. RTSTR_MAX if not 
 *                          known.
 */
static bool vbcppValidateCIdentifier(PVBCPP pThis, const char *pchIdentifier, size_t cchIdentifier)
{
    if (cchIdentifier == RTSTR_MAX)
        cchIdentifier = strlen(pchIdentifier);

    if (cchIdentifier == 0)
    {
        vbcppErrorPos(pThis, pchIdentifier, "Zero length identifier");
        return false;
    }

    if (!vbcppIsCIdentifierLeadChar(*pchIdentifier))
    {
        vbcppErrorPos(pThis, pchIdentifier, "Bad lead chararacter in identifier: '%.*s'", cchIdentifier, pchIdentifier);
        return false;
    }

    for (size_t off = 1; off < cchIdentifier; off++)
    {
        if (!vbcppIsCIdentifierChar(pchIdentifier[off]))
        {
            vbcppErrorPos(pThis, pchIdentifier + off, "Illegal chararacter in identifier: '%.*s' (#%zu)", cchIdentifier, pchIdentifier, off + 1);
            return false;
        }
    }

    return true;
}


/**
 * Frees a define. 
 *  
 * @returns VINF_SUCCESS (used when called by RTStrSpaceDestroy)
 * @param   pStr                Pointer to the VBCPPDEF::Core member.
 * @param   pvUser              Unused.
 */
static DECLCALLBACK(int) vbcppFreeDefine(PRTSTRSPACECORE pStr, void *pvUser)
{
    RTMemFree(pStr);
    NOREF(pvUser);
    return VINF_SUCCESS;
}


/**
 * Removes a define.
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pszDefine           The define name, no argument list or anything. 
 * @param   cchDefine           The length of the name. RTSTR_MAX is ok.
 */
static RTEXITCODE vbcppRemoveDefine(PVBCPP pThis, const char *pszDefine, size_t cchDefine)
{
    PRTSTRSPACECORE pHit = RTStrSpaceGetN(&pThis->StrSpace, pszDefine, cchDefine);
    if (pHit)
    {
        RTStrSpaceRemove(&pThis->StrSpace, pHit->pszString);
        vbcppFreeDefine(pHit, NULL);
    }
    return RTEXITCODE_SUCCESS;
}

/**
 * Inserts a define. 
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pDef                The define to insert.
 */
static RTEXITCODE vbcppInsertDefine(PVBCPP pThis, PVBCPPDEF pDef)
{
    if (RTStrSpaceInsert(&pThis->StrSpace, &pDef->Core))
        VBCPP_BITMAP_SET(pThis->bmDefined, *pDef->Core.pszString);
    else
    {
        RTMsgWarning("Redefining '%s'\n", pDef->Core.pszString);
        PVBCPPDEF pOld = (PVBCPPDEF)vbcppRemoveDefine(pThis, pDef->Core.pszString, pDef->Core.cchString);
        bool fRc = RTStrSpaceInsert(&pThis->StrSpace, &pDef->Core);
        Assert(fRc); Assert(pOld);
        vbcppFreeDefine(&pOld->Core, NULL);
    }

    return RTEXITCODE_SUCCESS;
}


/**
 * Adds a define. 
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pszDefine           The define name, no parameter list.
 * @param   cchDefine           The length of the name.
 * @param   pszParams           The parameter list. 
 * @param   cchParams           The length of the parameter list. 
 * @param   pszValue            The value. 
 * @param   cchDefine           The length of the value.
 */
static RTEXITCODE vbcppAddDefineFn(PVBCPP pThis, const char *pszDefine, size_t cchDefine, 
                                   const char *pszParams, size_t cchParams,
                                   const char *pszValue, size_t cchValue)

{
    Assert(RTStrNLen(pszDefine, cchDefine) == cchDefine);
    Assert(RTStrNLen(pszParams, cchParams) == cchParams);
    Assert(RTStrNLen(pszValue,  cchValue)  == cchValue);

    /* 
     * Determin the number of arguments and how much space their names
     * requires.  Performing syntax validation while parsing.
     */ 
    uint32_t cchArgNames = 0;
    uint32_t cArgs       = 0;
    for (size_t off = 0; off < cchParams; off++)
    {
        /* Skip blanks and maybe one comma. */
        bool fIgnoreComma = cArgs != 0;
        while (off < cchParams)
        {
            if (!RT_C_IS_SPACE(pszParams[off]))
            {
                if (pszParams[off] != ',' || !fIgnoreComma)
                {
                    if (vbcppIsCIdentifierLeadChar(pszParams[off]))
                        break;
                    /** @todo variadic macros. */
                    return vbcppErrorPos(pThis, &pszParams[off], "Unexpected character");
                }
                fIgnoreComma = false;
            }
            off++;
        }
        if (off >= cchParams)
            break;

        /* Found and argument. First character is already validated. */
        cArgs++;
        cchArgNames += 2;
        off++;
        while (   off < cchParams
               && vbcppIsCIdentifierChar(pszParams[off]))
            off++, cchArgNames++;
    }

    /*
     * Allocate a structure.
     */
    size_t    cbDef = RT_OFFSETOF(VBCPPDEF, szValue[cchValue + 1 + cchDefine + 1 + cchArgNames])
                    + sizeof(const char *) * cArgs;
    cbDef = RT_ALIGN_Z(cbDef, sizeof(const char *));
    PVBCPPDEF pDef  = (PVBCPPDEF)RTMemAlloc(cbDef);
    if (!pDef)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "out of memory");

    char *pszDst = &pDef->szValue[cchValue + 1];
    pDef->Core.pszString = pszDst;
    memcpy(pszDst, pszDefine, cchDefine);
    pszDst += cchDefine;
    *pszDst++ = '\0';
    pDef->fFunction = true;
    pDef->fVarArg   = false;
    pDef->cArgs     = cArgs;
    pDef->papszArgs = (const char **)((uintptr_t)pDef + cbDef - sizeof(const char *) * cArgs);
    VBCPP_BITMAP_EMPTY(pDef->bmArgs);
    memcpy(pDef->szValue, pszValue, cchValue);
    pDef->szValue[cchValue] = '\0';

    /*
     * Set up the arguments.
     */
    uint32_t iArg = 0;
    for (size_t off = 0; off < cchParams; off++)
    {
        /* Skip blanks and maybe one comma. */
        bool fIgnoreComma = cArgs != 0;
        while (off < cchParams)
        {
            if (!RT_C_IS_SPACE(pszParams[off]))
            {
                if (pszParams[off] != ',' || !fIgnoreComma)
                    break;
                fIgnoreComma = false;
            }
            off++;
        }
        if (off >= cchParams)
            break;

        /* Found and argument. First character is already validated. */
        pDef->papszArgs[iArg] = pszDst;
        do
        {
            *pszDst++ = pszParams[off++];
        } while (   off < cchParams
                 && vbcppIsCIdentifierChar(pszParams[off]));
        *pszDst++ = '\0';
        iArg++;
    }
    Assert((uintptr_t)pszDst <= (uintptr_t)pDef->papszArgs);

    return vbcppInsertDefine(pThis, pDef);
}


/**
 * Adds a define. 
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE + msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pszDefine           The define name and optionally the argument 
 *                              list.
 * @param   cchDefine           The length of the name. RTSTR_MAX is ok.
 * @param   pszValue            The value. 
 * @param   cchDefine           The length of the value. RTSTR_MAX is ok.
 */
static RTEXITCODE vbcppAddDefine(PVBCPP pThis, const char *pszDefine, size_t cchDefine, 
                                 const char *pszValue, size_t cchValue)
{
    /*
     * We need the lengths. Trim the input.
     */
    if (cchDefine == RTSTR_MAX)
        cchDefine = strlen(pszDefine);
    while (cchDefine > 0 && RT_C_IS_SPACE(*pszDefine))
        pszDefine++, cchDefine--;
    while (cchDefine > 0 && RT_C_IS_SPACE(pszDefine[cchDefine - 1]))
        cchDefine--;
    if (!cchDefine)
        return vbcppErrorPos(pThis, pszDefine, "The define has no name");

    if (cchValue == RTSTR_MAX)
        cchValue = strlen(pszValue);
    while (cchValue > 0 && RT_C_IS_SPACE(*pszValue))
        pszValue++, cchValue--;
    while (cchValue > 0 && RT_C_IS_SPACE(pszValue[cchValue - 1]))
        cchValue--;

    /*
     * Arguments make the job a bit more annoying.  Handle that elsewhere
     */
    const char *pszParams = (const char *)memchr(pszDefine, '(', cchDefine);
    if (pszParams)
    {
        size_t cchParams = pszDefine + cchDefine - pszParams;
        cchDefine -= cchParams;
        if (!vbcppValidateCIdentifier(pThis, pszDefine, cchDefine))
            return RTEXITCODE_FAILURE;
        if (pszParams[cchParams - 1] != ')')
            return vbcppErrorPos(pThis, pszParams + cchParams - 1, "Missing closing parenthesis");
        pszParams++;
        cchParams -= 2;
        return vbcppAddDefineFn(pThis, pszDefine, cchDefine, pszParams, cchParams, pszValue, cchValue);
    }

    /*
     * Simple define, no arguments.
     */
    if (vbcppValidateCIdentifier(pThis, pszDefine, cchDefine))
        return RTEXITCODE_FAILURE;

    PVBCPPDEF pDef = (PVBCPPDEF)RTMemAlloc(RT_OFFSETOF(VBCPPDEF, szValue[cchValue + 1 + cchDefine + 1]));
    if (!pDef)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "out of memory");

    pDef->Core.pszString = &pDef->szValue[cchValue + 1];
    memcpy((char *)pDef->Core.pszString, pszDefine, cchDefine);
    ((char *)pDef->Core.pszString)[cchDefine] = '\0';
    pDef->fFunction = false;
    pDef->fVarArg   = false;
    pDef->cArgs     = 0;
    pDef->papszArgs = NULL;
    VBCPP_BITMAP_EMPTY(pDef->bmArgs);
    memcpy(pDef->szValue, pszValue, cchValue);
    pDef->szValue[cchValue] = '\0';

    return vbcppInsertDefine(pThis, pDef);
}


/**
 * Adds an include directory.
 *  
 * @returns Program exit code, with error message on failure.
 * @param   pThis               The C preprocessor instance.
 * @param   pszDir              The directory to add.
 */
static RTEXITCODE vbcppAddInclude(PVBCPP pThis, const char *pszDir)
{
    uint32_t cIncludes = pThis->cIncludes;
    if (cIncludes >= _64K)
        return vbcppError(pThis, "Too many include directories");

    void *pv = RTMemRealloc(pThis->papszIncludes, (cIncludes + 1) * sizeof(char **));
    if (!pv)
        return vbcppError(pThis, "No memory for include directories");
    pThis->papszIncludes = (char **)pv;

    int rc = RTStrDupEx(&pThis->papszIncludes[cIncludes], pszDir);
    if (RT_FAILURE(rc))
        return vbcppError(pThis, "No string memory for include directories");

    pThis->cIncludes = cIncludes + 1;
    return RTEXITCODE_SUCCESS;
}


/**
 * Parses the command line options. 
 *  
 * @returns Program exit code. Exit on non-success or if *pfExit is set.
 * @param   pThis               The C preprocessor instance.
 * @param   argc                The argument count.
 * @param   argv                The argument vector.
 * @param   pfExit              Pointer to the exit indicator.
 */
static RTEXITCODE vbcppParseOptions(PVBCPP pThis, int argc, char **argv, bool *pfExit)
{
    RTEXITCODE rcExit;

    *pfExit = false;

    /*
     * Option config.
     */
    static RTGETOPTDEF const s_aOpts[] =
    {
        { "--define",                   'D',                    RTGETOPT_REQ_STRING },
        { "--include-dir",              'I',                    RTGETOPT_REQ_STRING },
        { "--undefine",                 'U',                    RTGETOPT_REQ_STRING },
        { "--keep-comments",            'C',                    RTGETOPT_REQ_NOTHING },
        { "--strip-comments",           'c',                    RTGETOPT_REQ_NOTHING },
        { "--D-strip",                  'd',                    RTGETOPT_REQ_NOTHING },
    };

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetOptState;
    int rc = RTGetOptInit(&GetOptState, argc, argv, &s_aOpts[0], RT_ELEMENTS(s_aOpts), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertReleaseRCReturn(rc, RTEXITCODE_FAILURE);

    /*
     * Process the options.
     */
    while ((rc = RTGetOpt(&GetOptState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'c':
                pThis->fKeepComments = false;
                break;

            case 'C':
                pThis->fKeepComments = false;
                break;

            case 'd':
                pThis->enmMode = kVBCppMode_SelectiveD;
                pThis->fKeepComments = true;
                break;

            case 'D':
            {
                const char *pszEqual = strchr(ValueUnion.psz, '=');
                if (pszEqual)
                    rcExit = vbcppAddDefine(pThis, ValueUnion.psz, pszEqual - ValueUnion.psz, pszEqual + 1, RTSTR_MAX);
                else
                    rcExit = vbcppAddDefine(pThis, ValueUnion.psz, RTSTR_MAX, "1", 1);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;
            }

            case 'I':
                rcExit = vbcppAddInclude(pThis, ValueUnion.psz);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;

            case 'U':
                rcExit = vbcppRemoveDefine(pThis, ValueUnion.psz, RTSTR_MAX);
                break;

            case 'h':
                RTPrintf("No help yet, sorry\n");
                *pfExit = true;
                return RTEXITCODE_SUCCESS;

            case 'V':
            {
                /* The following is assuming that svn does it's job here. */
                static const char s_szRev[] = "$Revision: 41179 $";
                const char *psz = RTStrStripL(strchr(s_szRev, ' '));
                RTPrintf("r%.*s\n", strchr(psz, ' ') - psz, psz);
                *pfExit = true;
                return RTEXITCODE_SUCCESS;
            }

            case VINF_GETOPT_NOT_OPTION:
                if (!pThis->pszInput)
                    pThis->pszInput = ValueUnion.psz;
                else if (!pThis->pszOutput)
                    pThis->pszOutput = ValueUnion.psz;
                else 
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "too many file arguments");
                break;


            /*
             * Errors and bugs.
             */
            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    return RTEXITCODE_SUCCESS;
}


/**
 * Opens the input and output streams.
 *  
 * @returns Exit code.
 * @param   pThis               The C preprocessor instance.
 */
static RTEXITCODE vbcppOpenStreams(PVBCPP pThis)
{
    if (!pThis->pszInput)
        return vbcppError(pThis, "Preprocessing the standard input stream is currently not supported");

    int rc = ScmStreamInitForReading(&pThis->StrmInput, pThis->pszInput);
    if (RT_FAILURE(rc))
        return vbcppError(pThis, "ScmStreamInitForReading returned %Rrc when opening input file (%s)",
                          rc, pThis->pszInput);

    rc = ScmStreamInitForWriting(&pThis->StrmOutput, &pThis->StrmInput);
    if (RT_FAILURE(rc))
        return vbcppError(pThis, "ScmStreamInitForWriting returned %Rrc", rc);

    pThis->fStrmOutputValid = true;
    return RTEXITCODE_SUCCESS;
}


/**
 * Outputs a character. 
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   ch                  The character to output.
 */
static RTEXITCODE vbcppOutputCh(PVBCPP pThis, char ch)
{
    int rc = ScmStreamPutCh(&pThis->StrmOutput, ch);
    if (RT_SUCCESS(rc))
        return RTEXITCODE_SUCCESS;
    return vbcppError(pThis, "Output error %Rrc");
}


/**
 * Outputs a string.
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pch                 The string. 
 * @param   cch                 The number of characters to write. 
 */
static RTEXITCODE vbcppOutputWrite(PVBCPP pThis, const char *pch, size_t cch)
{
    int rc = ScmStreamWrite(&pThis->StrmOutput, pch, cch);
    if (RT_SUCCESS(rc))
        return RTEXITCODE_SUCCESS;
    return vbcppError(pThis, "Output error %Rrc");
}


/**
 * Processes a multi-line comment.
 *  
 * Must either string the comment or keep it. If the latter, we must refrain 
 * from replacing C-words in it. 
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessMultiLineComment(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    /* The open comment sequence. */
    ScmStreamGetCh(pStrmInput);         /* '*' */
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    if (pThis->fKeepComments)
        rcExit = vbcppOutputWrite(pThis, "/*", 2);

    /* The comment.*/
    unsigned ch;
    while (   rcExit == RTEXITCODE_SUCCESS
           && (ch = ScmStreamGetCh(pStrmInput)) != ~(unsigned)0 )
    {
        if (ch == '*')
        {
            /* Closing sequence? */
            unsigned ch2 = ScmStreamPeekCh(pStrmInput);
            if (ch2 == '/')
            {
                ScmStreamGetCh(pStrmInput);
                if (pThis->fKeepComments)
                    rcExit = vbcppOutputWrite(pThis, "*/", 2);
                break;
            }
        }

        if (pThis->fKeepComments || ch == '\r' || ch == '\n')
        {
            rcExit = vbcppOutputCh(pThis, ch);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;

            /* Reset the maybe-preprocessor-line indicator when necessary. */
            if (ch == '\r' || ch == '\n')
                pThis->fMaybePreprocessorLine = true;
        }
    }
    return rcExit;
}


/**
 * Processes a single line comment. 
 *  
 * Must either string the comment or keep it. If the latter, we must refrain 
 * from replacing C-words in it. 
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessOneLineComment(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    RTEXITCODE  rcExit;
    SCMEOL      enmEol;
    size_t      cchLine;
    const char *pszLine = ScmStreamGetLine(pStrmInput, &cchLine, &enmEol); Assert(pszLine);
    pszLine--; cchLine++;               /* unfetching the first slash. */
    for (;;)
    {
        if (pThis->fKeepComments)
            rcExit = vbcppOutputWrite(pThis, pszLine, cchLine + enmEol);
        else
            rcExit = vbcppOutputWrite(pThis, pszLine + cchLine, enmEol);
        if (rcExit != RTEXITCODE_SUCCESS)
            break;
        if (   cchLine == 0
            || pszLine[cchLine - 1] != '\\')
            break;

        pszLine = ScmStreamGetLine(pStrmInput, &cchLine, &enmEol);
        if (!pszLine)
            break;
    }
    pThis->fMaybePreprocessorLine = true;
    return rcExit;
}


/**
 * Processes a double quoted string. 
 *  
 * Must not replace any C-words in strings. 
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessDoubleQuotedString(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    RTEXITCODE rcExit = vbcppOutputCh(pThis, '"');
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        bool fEscaped = false;
        for (;;)
        {
            unsigned ch = ScmStreamGetCh(pStrmInput);
            if (ch == ~(unsigned)0)
            {
                rcExit = vbcppError(pThis, "Unterminated double quoted string");
                break;
            }

            rcExit = vbcppOutputCh(pThis, ch);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;

            if (ch == '"' && !fEscaped)
                break;
            fEscaped = !fEscaped && ch == '\\';
        } 
    }
    return rcExit;
}


/**
 * Processes a single quoted litteral.
 *  
 * Must not replace any C-words in strings. 
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessSingledQuotedString(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    RTEXITCODE rcExit = vbcppOutputCh(pThis, '\'');
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        bool fEscaped = false;
        for (;;)
        {
            unsigned ch = ScmStreamGetCh(pStrmInput);
            if (ch == ~(unsigned)0)
            {
                rcExit = vbcppError(pThis, "Unterminated singled quoted string");
                break;
            }

            rcExit = vbcppOutputCh(pThis, ch);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;

            if (ch == '\'' && !fEscaped)
                break;
            fEscaped = !fEscaped && ch == '\\';
        } 
    }
    return rcExit;
}


/**
 * Processes a preprocessor directive.
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 */
static RTEXITCODE vbcppProcessDirective(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
#if 0
    size_t const offStart = ScmStreamTell(pStrmInput);

    /* 
     * Skip spaces.
     */
    unsigned chPrev = ~(unsigned)0;
    unsigned ch;                 
    while ((ch = ScmStreamGetCh(pStrmInput)) != ~(unsigned)0)
    {
        if (!RT_C_IS_SPACE(ch))
        {
            if ()
            {
            }
        }
        ch = chPrev;
    }
#endif
    return vbcppError(pThis, "Not implemented");
}


/**
 * Processes a C word, possibly replacing it with a definition.
 *  
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE+msg.
 * @param   pThis               The C preprocessor instance.
 * @param   pStrmInput          The input stream.
 * @param   ch                  The first character.
 */
static RTEXITCODE vbcppProcessCWord(PVBCPP pThis, PSCMSTREAM pStrmInput, char ch)
{
    /** @todo Implement this... */
    return vbcppOutputCh(pThis, ch);
}


/**
 * Does the actually preprocessing of the input file.
 *  
 * @returns Exit code.
 * @param   pThis               The C preprocessor instance. 
 * @param   pStrmInput          The input stream. 
 */
static RTEXITCODE vbcppPreprocess(PVBCPP pThis, PSCMSTREAM pStrmInput)
{
    /* 
     * Push.
     */
    PSCMSTREAM pStrmInputOld = pThis->pCurStrmInput;
    pThis->pCurStrmInput = pStrmInput;
    pThis->fMaybePreprocessorLine = true;

    /* 
     * Parse.
     */ 
    RTEXITCODE  rcExit = RTEXITCODE_SUCCESS;
    unsigned    ch;
    while ((ch = ScmStreamGetCh(pStrmInput)) != ~(unsigned)0)
    {
        if (ch == '/')
        {
            ch = ScmStreamPeekCh(pStrmInput);
            if (ch == '*')
                rcExit = vbcppProcessMultiLineComment(pThis, pStrmInput);
            else if (ch == '/')
                rcExit = vbcppProcessOneLineComment(pThis, pStrmInput);
            else
            {
                pThis->fMaybePreprocessorLine = false;
                rcExit = vbcppOutputCh(pThis, '/');
            }
        }
        else if (ch == '#' && pThis->fMaybePreprocessorLine)
            rcExit = vbcppProcessDirective(pThis, pStrmInput);
        else if (ch == '\r' || ch == '\n')
        {
            pThis->fMaybePreprocessorLine = true;
            rcExit = vbcppOutputCh(pThis, ch);
        }
        else if (RT_C_IS_SPACE(ch))
            rcExit = vbcppOutputCh(pThis, ch);
        else
        {
            pThis->fMaybePreprocessorLine = false;
            if (ch == '"')
                rcExit = vbcppProcessDoubleQuotedString(pThis, pStrmInput);
            else if (ch == '\'')
                rcExit = vbcppProcessSingledQuotedString(pThis, pStrmInput);
            else if (vbcppIsCIdentifierLeadChar(ch))
                rcExit = vbcppProcessCWord(pThis, pStrmInput, ch);
            else
                rcExit = vbcppOutputCh(pThis, ch);
        }
        if (rcExit != RTEXITCODE_SUCCESS)
            break;
    }
    
    /*
     * Pop.
     */
    pThis->pCurStrmInput = pStrmInputOld;
    pThis->fMaybePreprocessorLine = true;
    return rcExit;
}


/**
 * Terminates the preprocessor. 
 *  
 * This may return failure if an error was delayed. 
 *  
 * @returns Exit code.
 * @param   pThis               The C preprocessor instance.
 */
static RTEXITCODE vbcppTerm(PVBCPP pThis)
{
    /*
     * Flush the output first.
     */
    if (pThis->fStrmOutputValid)
    {
        if (pThis->pszOutput)
        {
            int rc = ScmStreamWriteToFile(&pThis->StrmOutput, "%s", pThis->pszOutput);
            if (RT_FAILURE(rc))
                vbcppError(pThis, "ScmStreamWriteToFile failed with %Rrc when writing '%s'", rc, pThis->pszOutput);
        }
        else
        {
            int rc = ScmStreamWriteToStdOut(&pThis->StrmOutput);
            if (RT_FAILURE(rc))
                vbcppError(pThis, "ScmStreamWriteToStdOut failed with %Rrc", rc);
        }
    }

    /*
     * Cleanup.
     */
    ScmStreamDelete(&pThis->StrmInput);
    ScmStreamDelete(&pThis->StrmOutput);

    RTStrSpaceDestroy(&pThis->StrSpace, vbcppFreeDefine, NULL);
    pThis->StrSpace = NULL;

    uint32_t i = pThis->cIncludes;
    while (i-- > 0)
        RTStrFree(pThis->papszIncludes[i]);
    RTMemFree(pThis->papszIncludes);
    pThis->papszIncludes = NULL;

    return pThis->rcExit;
}



int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Do the job.  The code says it all.
     */
    VBCPP This;
    vbcppInit(&This);
    bool fExit;
    RTEXITCODE rcExit = vbcppParseOptions(&This, argc, argv, &fExit);
    if (!fExit && rcExit == RTEXITCODE_SUCCESS)
    {
        rcExit = vbcppOpenStreams(&This);
        if (rcExit == RTEXITCODE_SUCCESS)
            rcExit = vbcppPreprocess(&This, &This.StrmInput);
    }

    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = vbcppTerm(&This);
    else
        vbcppTerm(&This);
    return rcExit;
}


