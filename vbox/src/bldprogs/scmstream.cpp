/* $Id: scmstream.cpp 40554 2012-03-20 18:07:34Z vboxsync $ */
/** @file
 * IPRT Testcase / Tool - Source Code Massager Stream Code.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include "scmstream.h"


/**
 * Initializes the stream structure.
 *
 * @param   pStream             The stream structure.
 * @param   fWriteOrRead        The value of the fWriteOrRead stream member.
 */
static void scmStreamInitInternal(PSCMSTREAM pStream, bool fWriteOrRead)
{
    pStream->pch                = NULL;
    pStream->off                = 0;
    pStream->cb                 = 0;
    pStream->cbAllocated        = 0;

    pStream->paLines            = NULL;
    pStream->iLine              = 0;
    pStream->cLines             = 0;
    pStream->cLinesAllocated    = 0;

    pStream->fWriteOrRead       = fWriteOrRead;
    pStream->fFileMemory        = false;
    pStream->fFullyLineated     = false;

    pStream->rc                 = VINF_SUCCESS;
}

/**
 * Initialize an input stream.
 *
 * @returns IPRT status code.
 * @param   pStream             The stream to initialize.
 * @param   pszFilename         The file to take the stream content from.
 */
int ScmStreamInitForReading(PSCMSTREAM pStream, const char *pszFilename)
{
    scmStreamInitInternal(pStream, false /*fWriteOrRead*/);

    void *pvFile;
    size_t cbFile;
    int rc = pStream->rc = RTFileReadAll(pszFilename, &pvFile, &cbFile);
    if (RT_SUCCESS(rc))
    {
        pStream->pch            = (char *)pvFile;
        pStream->cb             = cbFile;
        pStream->cbAllocated    = cbFile;
        pStream->fFileMemory    = true;
    }
    return rc;
}

/**
 * Initialize an output stream.
 *
 * @returns IPRT status code
 * @param   pStream             The stream to initialize.
 * @param   pRelatedStream      Pointer to a related stream.  NULL is fine.
 */
int ScmStreamInitForWriting(PSCMSTREAM pStream, PCSCMSTREAM pRelatedStream)
{
    scmStreamInitInternal(pStream, true /*fWriteOrRead*/);

    /* allocate stuff */
    size_t cbEstimate = pRelatedStream
                      ? pRelatedStream->cb + pRelatedStream->cb / 10
                      : _64K;
    cbEstimate = RT_ALIGN(cbEstimate, _4K);
    pStream->pch = (char *)RTMemAlloc(cbEstimate);
    if (pStream->pch)
    {
        size_t cLinesEstimate = pRelatedStream && pRelatedStream->fFullyLineated
                              ? pRelatedStream->cLines + pRelatedStream->cLines / 10
                              : cbEstimate / 24;
        cLinesEstimate = RT_ALIGN(cLinesEstimate, 512);
        pStream->paLines = (PSCMSTREAMLINE)RTMemAlloc(cLinesEstimate * sizeof(SCMSTREAMLINE));
        if (pStream->paLines)
        {
            pStream->paLines[0].off     = 0;
            pStream->paLines[0].cch     = 0;
            pStream->paLines[0].enmEol  = SCMEOL_NONE;
            pStream->cbAllocated        = cbEstimate;
            pStream->cLinesAllocated    = cLinesEstimate;
            return VINF_SUCCESS;
        }

        RTMemFree(pStream->pch);
        pStream->pch = NULL;
    }
    return pStream->rc = VERR_NO_MEMORY;
}

/**
 * Frees the resources associated with the stream.
 *
 * Nothing is happens to whatever the stream was initialized from or dumped to.
 *
 * @param   pStream             The stream to delete.
 */
void ScmStreamDelete(PSCMSTREAM pStream)
{
    if (pStream->pch)
    {
        if (pStream->fFileMemory)
            RTFileReadAllFree(pStream->pch, pStream->cbAllocated);
        else
            RTMemFree(pStream->pch);
        pStream->pch = NULL;
    }
    pStream->cbAllocated = 0;

    if (pStream->paLines)
    {
        RTMemFree(pStream->paLines);
        pStream->paLines = NULL;
    }
    pStream->cLinesAllocated = 0;
}

/**
 * Get the stream status code.
 *
 * @returns IPRT status code.
 * @param   pStream             The stream.
 */
int ScmStreamGetStatus(PCSCMSTREAM pStream)
{
    return pStream->rc;
}

/**
 * Grows the buffer of a write stream.
 *
 * @returns IPRT status code.
 * @param   pStream             The stream.  Must be in write mode.
 * @param   cbAppending         The minimum number of bytes to grow the buffer
 *                              with.
 */
static int scmStreamGrowBuffer(PSCMSTREAM pStream, size_t cbAppending)
{
    size_t cbAllocated = pStream->cbAllocated;
    cbAllocated += RT_MAX(0x1000 + cbAppending, cbAllocated);
    cbAllocated = RT_ALIGN(cbAllocated, 0x1000);
    void *pvNew;
    if (!pStream->fFileMemory)
    {
        pvNew = RTMemRealloc(pStream->pch, cbAllocated);
        if (!pvNew)
            return pStream->rc = VERR_NO_MEMORY;
    }
    else
    {
        pvNew = RTMemDupEx(pStream->pch, pStream->off, cbAllocated - pStream->off);
        if (!pvNew)
            return pStream->rc = VERR_NO_MEMORY;
        RTFileReadAllFree(pStream->pch, pStream->cbAllocated);
        pStream->fFileMemory = false;
    }
    pStream->pch = (char *)pvNew;
    pStream->cbAllocated = cbAllocated;

    return VINF_SUCCESS;
}

/**
 * Grows the line array of a stream.
 *
 * @returns IPRT status code.
 * @param   pStream             The stream.
 * @param   iMinLine            Minimum line number.
 */
static int scmStreamGrowLines(PSCMSTREAM pStream, size_t iMinLine)
{
    size_t cLinesAllocated = pStream->cLinesAllocated;
    cLinesAllocated += RT_MAX(512 + iMinLine, cLinesAllocated);
    cLinesAllocated = RT_ALIGN(cLinesAllocated, 512);
    void *pvNew = RTMemRealloc(pStream->paLines, cLinesAllocated * sizeof(SCMSTREAMLINE));
    if (!pvNew)
        return pStream->rc = VERR_NO_MEMORY;

    pStream->paLines = (PSCMSTREAMLINE)pvNew;
    pStream->cLinesAllocated = cLinesAllocated;
    return VINF_SUCCESS;
}

/**
 * Rewinds the stream and sets the mode to read.
 *
 * @param   pStream             The stream.
 */
void ScmStreamRewindForReading(PSCMSTREAM pStream)
{
    pStream->off          = 0;
    pStream->iLine        = 0;
    pStream->fWriteOrRead = false;
    pStream->rc           = VINF_SUCCESS;
}

/**
 * Rewinds the stream and sets the mode to write.
 *
 * @param   pStream             The stream.
 */
void ScmStreamRewindForWriting(PSCMSTREAM pStream)
{
    pStream->off            = 0;
    pStream->iLine          = 0;
    pStream->cLines         = 0;
    pStream->fWriteOrRead   = true;
    pStream->fFullyLineated = true;
    pStream->rc             = VINF_SUCCESS;
}

/**
 * Checks if it's a text stream.
 *
 * Not 100% proof.
 *
 * @returns true if it probably is a text file, false if not.
 * @param   pStream             The stream. Write or read, doesn't matter.
 */
bool ScmStreamIsText(PSCMSTREAM pStream)
{
    if (RTStrEnd(pStream->pch, pStream->cb))
        return false;
    if (!pStream->cb)
        return false;
    return true;
}

/**
 * Performs an integrity check of the stream.
 *
 * @returns IPRT status code.
 * @param   pStream             The stream.
 */
int ScmStreamCheckItegrity(PSCMSTREAM pStream)
{
    /*
     * Perform sanity checks.
     */
    size_t const cbFile = pStream->cb;
    for (size_t iLine = 0; iLine < pStream->cLines; iLine++)
    {
        size_t offEol = pStream->paLines[iLine].off + pStream->paLines[iLine].cch;
        AssertReturn(offEol + pStream->paLines[iLine].enmEol <= cbFile, VERR_INTERNAL_ERROR_2);
        switch (pStream->paLines[iLine].enmEol)
        {
            case SCMEOL_LF:
                AssertReturn(pStream->pch[offEol] == '\n', VERR_INTERNAL_ERROR_3);
                break;
            case SCMEOL_CRLF:
                AssertReturn(pStream->pch[offEol] == '\r', VERR_INTERNAL_ERROR_3);
                AssertReturn(pStream->pch[offEol + 1] == '\n', VERR_INTERNAL_ERROR_3);
                break;
            case SCMEOL_NONE:
                AssertReturn(iLine + 1 >= pStream->cLines, VERR_INTERNAL_ERROR_4);
                break;
            default:
                AssertReturn(iLine + 1 >= pStream->cLines, VERR_INTERNAL_ERROR_5);
        }
    }
    return VINF_SUCCESS;
}

/**
 * Writes the stream to a file.
 *
 * @returns IPRT status code
 * @param   pStream             The stream.
 * @param   pszFilenameFmt      The filename format string.
 * @param   ...                 Format arguments.
 */
int ScmStreamWriteToFile(PSCMSTREAM pStream, const char *pszFilenameFmt, ...)
{
    int rc;

#ifdef RT_STRICT
    /*
     * Check that what we're going to write makes sense first.
     */
    rc = ScmStreamCheckItegrity(pStream);
    if (RT_FAILURE(rc))
        return rc;
#endif

    /*
     * Do the actual writing.
     */
    RTFILE hFile;
    va_list va;
    va_start(va, pszFilenameFmt);
    rc = RTFileOpenV(&hFile, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_WRITE, pszFilenameFmt, va);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileWrite(hFile, pStream->pch, pStream->cb, NULL);
        RTFileClose(hFile);
    }
    return rc;
}

/**
 * Worker for ScmStreamGetLine that builds the line number index while parsing
 * the stream.
 *
 * @returns Same as SCMStreamGetLine.
 * @param   pStream             The stream.  Must be in read mode.
 * @param   pcchLine            Where to return the line length.
 * @param   penmEol             Where to return the kind of end of line marker.
 */
static const char *scmStreamGetLineInternal(PSCMSTREAM pStream, size_t *pcchLine, PSCMEOL penmEol)
{
    AssertReturn(!pStream->fWriteOrRead, NULL);
    if (RT_FAILURE(pStream->rc))
        return NULL;

    size_t off = pStream->off;
    size_t cb  = pStream->cb;
    if (RT_UNLIKELY(off >= cb))
    {
        pStream->fFullyLineated = true;
        return NULL;
    }

    size_t iLine = pStream->iLine;
    if (RT_UNLIKELY(iLine >= pStream->cLinesAllocated))
    {
        int rc = scmStreamGrowLines(pStream, iLine);
        if (RT_FAILURE(rc))
            return NULL;
    }
    pStream->paLines[iLine].off = off;

    cb -= off;
    const char *pchRet = &pStream->pch[off];
    const char *pch = (const char *)memchr(pchRet, '\n', cb);
    if (RT_LIKELY(pch))
    {
        cb = pch - pchRet;
        pStream->off = off + cb + 1;
        if (   cb < 1
            || pch[-1] != '\r')
            pStream->paLines[iLine].enmEol = *penmEol = SCMEOL_LF;
        else
        {
            pStream->paLines[iLine].enmEol = *penmEol = SCMEOL_CRLF;
            cb--;
        }
    }
    else
    {
        pStream->off = off + cb;
        pStream->paLines[iLine].enmEol = *penmEol = SCMEOL_NONE;
    }
    *pcchLine = cb;
    pStream->paLines[iLine].cch = cb;
    pStream->cLines = pStream->iLine = ++iLine;

    return pchRet;
}

/**
 * Internal worker that delineates a stream.
 *
 * @returns IPRT status code.
 * @param   pStream             The stream.  Caller must check that it is in
 *                              read mode.
 */
static int scmStreamLineate(PSCMSTREAM pStream)
{
    /* Save the stream position. */
    size_t const offSaved   = pStream->off;
    size_t const iLineSaved = pStream->iLine;

    /* Get each line. */
    size_t cchLine;
    SCMEOL enmEol;
    while (scmStreamGetLineInternal(pStream, &cchLine, &enmEol))
        /* nothing */;
    Assert(RT_FAILURE(pStream->rc) || pStream->fFullyLineated);

    /* Restore the position */
    pStream->off   = offSaved;
    pStream->iLine = iLineSaved;

    return pStream->rc;
}

/**
 * Get the current stream position as an byte offset.
 *
 * @returns The current byte offset
 * @param   pStream             The stream.
 */
size_t ScmStreamTell(PSCMSTREAM pStream)
{
    return pStream->off;
}

/**
 * Get the current stream position as a line number.
 *
 * @returns The current line (0-based).
 * @param   pStream             The stream.
 */
size_t ScmStreamTellLine(PSCMSTREAM pStream)
{
    return pStream->iLine;
}

/**
 * Get the current stream size in bytes.
 *
 * @returns Count of bytes.
 * @param   pStream             The stream.
 */
size_t ScmStreamSize(PSCMSTREAM pStream)
{
    return pStream->cb;
}

/**
 * Gets the number of lines in the stream.
 *
 * @returns The number of lines.
 * @param   pStream             The stream.
 */
size_t ScmStreamCountLines(PSCMSTREAM pStream)
{
    if (!pStream->fFullyLineated)
        scmStreamLineate(pStream);
    return pStream->cLines;
}

/**
 * Seeks to a given byte offset in the stream.
 *
 * @returns IPRT status code.
 * @retval  VERR_SEEK if the new stream position is the middle of an EOL marker.
 *          This is a temporary restriction.
 *
 * @param   pStream             The stream.  Must be in read mode.
 * @param   offAbsolute         The offset to seek to.  If this is beyond the
 *                              end of the stream, the position is set to the
 *                              end.
 */
int ScmStreamSeekAbsolute(PSCMSTREAM pStream, size_t offAbsolute)
{
    AssertReturn(!pStream->fWriteOrRead, VERR_ACCESS_DENIED);
    if (RT_FAILURE(pStream->rc))
        return pStream->rc;

    /* Must be fully delineated. (lazy bird) */
    if (RT_UNLIKELY(!pStream->fFullyLineated))
    {
        int rc = scmStreamLineate(pStream);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* Ok, do the job. */
    if (offAbsolute < pStream->cb)
    {
        /** @todo Should do a binary search here, but I'm too darn lazy tonight. */
        pStream->off = ~(size_t)0;
        for (size_t i = 0; i < pStream->cLines; i++)
        {
            if (offAbsolute < pStream->paLines[i].off + pStream->paLines[i].cch + pStream->paLines[i].enmEol)
            {
                pStream->off   = offAbsolute;
                pStream->iLine = i;
                if (offAbsolute > pStream->paLines[i].off + pStream->paLines[i].cch)
                    return pStream->rc = VERR_SEEK;
                break;
            }
        }
        AssertReturn(pStream->off != ~(size_t)0, pStream->rc = VERR_INTERNAL_ERROR_3);
    }
    else
    {
        pStream->off   = pStream->cb;
        pStream->iLine = pStream->cLines;
    }
    return VINF_SUCCESS;
}


/**
 * Seeks a number of bytes relative to the current stream position.
 *
 * @returns IPRT status code.
 * @retval  VERR_SEEK if the new stream position is the middle of an EOL marker.
 *          This is a temporary restriction.
 *
 * @param   pStream             The stream.  Must be in read mode.
 * @param   offRelative         The offset to seek to.  A negative offset
 *                              rewinds and positive one fast forwards the
 *                              stream.  Will quietly stop at the beginning and
 *                              end of the stream.
 */
int ScmStreamSeekRelative(PSCMSTREAM pStream, ssize_t offRelative)
{
    size_t offAbsolute;
    if (offRelative >= 0)
        offAbsolute = pStream->off + offRelative;
    else if ((size_t)-offRelative <= pStream->off)
        offAbsolute = pStream->off + offRelative;
    else
        offAbsolute = 0;
    return ScmStreamSeekAbsolute(pStream, offAbsolute);
}

/**
 * Seeks to a given line in the stream.
 *
 * @returns IPRT status code.
 *
 * @param   pStream             The stream.  Must be in read mode.
 * @param   iLine               The line to seek to.  If this is beyond the end
 *                              of the stream, the position is set to the end.
 */
int ScmStreamSeekByLine(PSCMSTREAM pStream, size_t iLine)
{
    AssertReturn(!pStream->fWriteOrRead, VERR_ACCESS_DENIED);
    if (RT_FAILURE(pStream->rc))
        return pStream->rc;

    /* Must be fully delineated. (lazy bird) */
    if (RT_UNLIKELY(!pStream->fFullyLineated))
    {
        int rc = scmStreamLineate(pStream);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* Ok, do the job. */
    if (iLine < pStream->cLines)
    {
        pStream->off   = pStream->paLines[iLine].off;
        pStream->iLine = iLine;
    }
    else
    {
        pStream->off   = pStream->cb;
        pStream->iLine = pStream->cLines;
    }
    return VINF_SUCCESS;
}

/**
 * Get a numbered line from the stream (changes the position).
 *
 * A line is always delimited by a LF character or the end of the stream.  The
 * delimiter is not included in returned line length, but instead returned via
 * the @a penmEol indicator.
 *
 * @returns Pointer to the first character in the line, not NULL terminated.
 *          NULL if the end of the stream has been reached or some problem
 *          occurred.
 *
 * @param   pStream             The stream.  Must be in read mode.
 * @param   iLine               The line to get (0-based).
 * @param   pcchLine            The length.
 * @param   penmEol             Where to return the end of line type indicator.
 */
const char *ScmStreamGetLineByNo(PSCMSTREAM pStream, size_t iLine, size_t *pcchLine, PSCMEOL penmEol)
{
    AssertReturn(!pStream->fWriteOrRead, NULL);
    if (RT_FAILURE(pStream->rc))
        return NULL;

    /* Make sure it's fully delineated so we can use the index. */
    if (RT_UNLIKELY(!pStream->fFullyLineated))
    {
        int rc = scmStreamLineate(pStream);
        if (RT_FAILURE(rc))
            return NULL;
    }

    /* End of stream? */
    if (RT_UNLIKELY(iLine >= pStream->cLines))
    {
        pStream->off   = pStream->cb;
        pStream->iLine = pStream->cLines;
        return NULL;
    }

    /* Get the data. */
    const char *pchRet = &pStream->pch[pStream->paLines[iLine].off];
    *pcchLine          = pStream->paLines[iLine].cch;
    *penmEol           = pStream->paLines[iLine].enmEol;

    /* update the stream position. */
    pStream->off       = pStream->paLines[iLine].off + pStream->paLines[iLine].cch + pStream->paLines[iLine].enmEol;
    pStream->iLine     = iLine + 1;

    return pchRet;
}

/**
 * Get a line from the stream.
 *
 * A line is always delimited by a LF character or the end of the stream.  The
 * delimiter is not included in returned line length, but instead returned via
 * the @a penmEol indicator.
 *
 * @returns Pointer to the first character in the line, not NULL terminated.
 *          NULL if the end of the stream has been reached or some problem
 *          occurred.
 *
 * @param   pStream             The stream.  Must be in read mode.
 * @param   pcchLine            The length.
 * @param   penmEol             Where to return the end of line type indicator.
 */
const char *ScmStreamGetLine(PSCMSTREAM pStream, size_t *pcchLine, PSCMEOL penmEol)
{
    /** @todo this doesn't work when pStream->off !=
     *        pStream->paLines[pStream->iLine-1].off. */
    if (!pStream->fFullyLineated)
        return scmStreamGetLineInternal(pStream, pcchLine, penmEol);
    return ScmStreamGetLineByNo(pStream, pStream->iLine, pcchLine, penmEol);
}


/**
 * Gets a character from the stream.
 *
 * @returns The next unsigned character in the stream.
 *          ~(unsigned)0 on failure.
 * @param   pStream             The stream.  Must be in read mode.
 */
unsigned ScmStreamGetCh(PSCMSTREAM pStream)
{
    /* Check stream state. */
    AssertReturn(!pStream->fWriteOrRead, ~(unsigned)0);
    if (RT_FAILURE(pStream->rc))
        return ~(unsigned)0;
    if (RT_UNLIKELY(!pStream->fFullyLineated))
    {
        int rc = scmStreamLineate(pStream);
        if (RT_FAILURE(rc))
            return ~(unsigned)0;
    }

    /* If there isn't enough stream left, fail already. */
    if (RT_UNLIKELY(pStream->off >= pStream->cb))
        return ~(unsigned)0;

    /* Read a character. */
    char ch = pStream->pch[pStream->off++];

    /* Advance the line indicator. */
    size_t iLine = pStream->iLine;
    if (pStream->off >= pStream->paLines[iLine].off + pStream->paLines[iLine].cch + pStream->paLines[iLine].enmEol)
        pStream->iLine++;

    return (unsigned)ch;
}


/**
 * Peeks at the next character from the stream.
 *
 * @returns The next unsigned character in the stream.
 *          ~(unsigned)0 on failure.
 * @param   pStream             The stream.  Must be in read mode.
 */
unsigned ScmStreamPeekCh(PSCMSTREAM pStream)
{
    /* Check stream state. */
    AssertReturn(!pStream->fWriteOrRead, ~(unsigned)0);
    if (RT_FAILURE(pStream->rc))
        return ~(unsigned)0;
    if (RT_UNLIKELY(!pStream->fFullyLineated))
    {
        int rc = scmStreamLineate(pStream);
        if (RT_FAILURE(rc))
            return ~(unsigned)0;
    }

    /* If there isn't enough stream left, fail already. */
    if (RT_UNLIKELY(pStream->off >= pStream->cb))
        return ~(unsigned)0;

    /* Peek at the next character. */
    char ch = pStream->pch[pStream->off++];
    return (unsigned)ch;
}


/**
 * Reads @a cbToRead bytes into @a pvBuf.
 *
 * Will fail if end of stream is encountered before the entire read has been
 * completed.
 *
 * @returns IPRT status code.
 * @retval  VERR_EOF if there isn't @a cbToRead bytes left to read.  Stream
 *          position will be unchanged.
 *
 * @param   pStream             The stream.  Must be in read mode.
 * @param   pvBuf               The buffer to read into.
 * @param   cbToRead            The number of bytes to read.
 */
int ScmStreamRead(PSCMSTREAM pStream, void *pvBuf, size_t cbToRead)
{
    AssertReturn(!pStream->fWriteOrRead, VERR_PERMISSION_DENIED);
    if (RT_FAILURE(pStream->rc))
        return pStream->rc;

    /* If there isn't enough stream left, fail already. */
    if (RT_UNLIKELY(pStream->cb - pStream->off < cbToRead))
        return VERR_EOF;

    /* Copy the data and simply seek to the new stream position. */
    memcpy(pvBuf, &pStream->pch[pStream->off], cbToRead);
    return ScmStreamSeekAbsolute(pStream, pStream->off + cbToRead);
}


/**
 * Checks if the given line is empty or full of white space.
 *
 * @returns true if white space only, false if not (or if non-existant).
 * @param   pStream             The stream.  Must be in read mode.
 * @param   iLine               The line in question.
 */
bool ScmStreamIsWhiteLine(PSCMSTREAM pStream, size_t iLine)
{
    SCMEOL      enmEol;
    size_t      cchLine;
    const char *pchLine = ScmStreamGetLineByNo(pStream, iLine, &cchLine, &enmEol);
    if (!pchLine)
        return false;
    while (cchLine && RT_C_IS_SPACE(*pchLine))
        pchLine++, cchLine--;
    return cchLine == 0;
}

/**
 * Try figure out the end of line style of the give stream.
 *
 * @returns Most likely end of line style.
 * @param   pStream             The stream.
 */
SCMEOL ScmStreamGetEol(PSCMSTREAM pStream)
{
    SCMEOL enmEol;
    if (pStream->cLines > 0)
        enmEol = pStream->paLines[0].enmEol;
    else if (pStream->cb == 0)
        enmEol = SCMEOL_NONE;
    else
    {
        const char *pchLF = (const char *)memchr(pStream->pch, '\n', pStream->cb);
        if (pchLF && pchLF != pStream->pch && pchLF[-1] == '\r')
            enmEol = SCMEOL_CRLF;
        else
            enmEol = SCMEOL_LF;
    }

    if (enmEol == SCMEOL_NONE)
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        enmEol = SCMEOL_CRLF;
#else
        enmEol = SCMEOL_LF;
#endif
    return enmEol;
}

/**
 * Get the end of line indicator type for a line.
 *
 * @returns The EOL indicator.  If the line isn't found, the default EOL
 *          indicator is return.
 * @param   pStream             The stream.
 * @param   iLine               The line (0-base).
 */
SCMEOL ScmStreamGetEolByLine(PSCMSTREAM pStream, size_t iLine)
{
    SCMEOL enmEol;
    if (iLine < pStream->cLines)
        enmEol = pStream->paLines[iLine].enmEol;
    else
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        enmEol = SCMEOL_CRLF;
#else
        enmEol = SCMEOL_LF;
#endif
    return enmEol;
}

/**
 * Appends a line to the stream.
 *
 * @returns IPRT status code.
 * @param   pStream             The stream.  Must be in write mode.
 * @param   pchLine             Pointer to the line.
 * @param   cchLine             Line length.
 * @param   enmEol              Which end of line indicator to use.
 */
int ScmStreamPutLine(PSCMSTREAM pStream, const char *pchLine, size_t cchLine, SCMEOL enmEol)
{
    AssertReturn(pStream->fWriteOrRead, VERR_ACCESS_DENIED);
    if (RT_FAILURE(pStream->rc))
        return pStream->rc;

    /*
     * Make sure the previous line has a new-line indicator.
     */
    size_t off   = pStream->off;
    size_t iLine = pStream->iLine;
    if (RT_UNLIKELY(   iLine != 0
                    && pStream->paLines[iLine - 1].enmEol == SCMEOL_NONE))
    {
        AssertReturn(pStream->paLines[iLine].cch == 0, VERR_INTERNAL_ERROR_3);
        SCMEOL enmEol2 = enmEol != SCMEOL_NONE ? enmEol : ScmStreamGetEol(pStream);
        if (RT_UNLIKELY(off + cchLine + enmEol + enmEol2 > pStream->cbAllocated))
        {
            int rc = scmStreamGrowBuffer(pStream, cchLine + enmEol + enmEol2);
            if (RT_FAILURE(rc))
                return rc;
        }
        if (enmEol2 == SCMEOL_LF)
            pStream->pch[off++] = '\n';
        else
        {
            pStream->pch[off++] = '\r';
            pStream->pch[off++] = '\n';
        }
        pStream->paLines[iLine - 1].enmEol = enmEol2;
        pStream->paLines[iLine].off = off;
        pStream->off = off;
        pStream->cb  = off;
    }

    /*
     * Ensure we've got sufficient buffer space.
     */
    if (RT_UNLIKELY(off + cchLine + enmEol > pStream->cbAllocated))
    {
        int rc = scmStreamGrowBuffer(pStream, cchLine + enmEol);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Add a line record.
     */
    if (RT_UNLIKELY(iLine + 1 >= pStream->cLinesAllocated))
    {
        int rc = scmStreamGrowLines(pStream, iLine);
        if (RT_FAILURE(rc))
            return rc;
    }

    pStream->paLines[iLine].cch    = off - pStream->paLines[iLine].off + cchLine;
    pStream->paLines[iLine].enmEol = enmEol;

    iLine++;
    pStream->cLines = iLine;
    pStream->iLine  = iLine;

    /*
     * Copy the line
     */
    memcpy(&pStream->pch[off], pchLine, cchLine);
    off += cchLine;
    if (enmEol == SCMEOL_LF)
        pStream->pch[off++] = '\n';
    else if (enmEol == SCMEOL_CRLF)
    {
        pStream->pch[off++] = '\r';
        pStream->pch[off++] = '\n';
    }
    pStream->off = off;
    pStream->cb  = off;

    /*
     * Start a new line.
     */
    pStream->paLines[iLine].off    = off;
    pStream->paLines[iLine].cch    = 0;
    pStream->paLines[iLine].enmEol = SCMEOL_NONE;

    return VINF_SUCCESS;
}

/**
 * Writes to the stream.
 *
 * @returns IPRT status code
 * @param   pStream             The stream.  Must be in write mode.
 * @param   pchBuf              What to write.
 * @param   cchBuf              How much to write.
 */
int ScmStreamWrite(PSCMSTREAM pStream, const char *pchBuf, size_t cchBuf)
{
    AssertReturn(pStream->fWriteOrRead, VERR_ACCESS_DENIED);
    if (RT_FAILURE(pStream->rc))
        return pStream->rc;

    /*
     * Ensure we've got sufficient buffer space.
     */
    size_t off = pStream->off;
    if (RT_UNLIKELY(off + cchBuf > pStream->cbAllocated))
    {
        int rc = scmStreamGrowBuffer(pStream, cchBuf);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Deal with the odd case where we've already pushed a line with SCMEOL_NONE.
     */
    size_t iLine = pStream->iLine;
    if (RT_UNLIKELY(   iLine > 0
                    && pStream->paLines[iLine - 1].enmEol == SCMEOL_NONE))
    {
        iLine--;
        pStream->cLines = iLine;
        pStream->iLine  = iLine;
    }

    /*
     * Deal with lines.
     */
    const char *pchLF = (const char *)memchr(pchBuf, '\n', cchBuf);
    if (!pchLF)
        pStream->paLines[iLine].cch += cchBuf;
    else
    {
        const char *pchLine = pchBuf;
        for (;;)
        {
            if (RT_UNLIKELY(iLine + 1 >= pStream->cLinesAllocated))
            {
                int rc = scmStreamGrowLines(pStream, iLine);
                if (RT_FAILURE(rc))
                {
                    iLine = pStream->iLine;
                    pStream->paLines[iLine].cch    = off - pStream->paLines[iLine].off;
                    pStream->paLines[iLine].enmEol = SCMEOL_NONE;
                    return rc;
                }
            }

            size_t cchLine = pchLF - pchLine;
            if (   cchLine
                ?  pchLF[-1] != '\r'
                :     !pStream->paLines[iLine].cch
                   || pStream->pch[pStream->paLines[iLine].off + pStream->paLines[iLine].cch - 1] != '\r')
                pStream->paLines[iLine].enmEol = SCMEOL_LF;
            else
            {
                pStream->paLines[iLine].enmEol = SCMEOL_CRLF;
                cchLine--;
            }
            pStream->paLines[iLine].cch += cchLine;

            iLine++;
            size_t offBuf = pchLF + 1 - pchBuf;
            pStream->paLines[iLine].off    = off + offBuf;
            pStream->paLines[iLine].cch    = 0;
            pStream->paLines[iLine].enmEol = SCMEOL_NONE;

            size_t cchLeft = cchBuf - offBuf;
            pchLF = (const char *)memchr(pchLF + 1, '\n', cchLeft);
            if (!pchLF)
            {
                pStream->paLines[iLine].cch = cchLeft;
                break;
            }
        }

        pStream->iLine  = iLine;
        pStream->cLines = iLine;
    }

    /*
     * Copy the data and update position and size.
     */
    memcpy(&pStream->pch[off], pchBuf, cchBuf);
    off += cchBuf;
    pStream->off = off;
    pStream->cb  = off;

    return VINF_SUCCESS;
}

/**
 * Write a character to the stream.
 *
 * @returns IPRT status code
 * @param   pStream             The stream.  Must be in write mode.
 * @param   pchBuf              What to write.
 * @param   cchBuf              How much to write.
 */
int ScmStreamPutCh(PSCMSTREAM pStream, char ch)
{
    AssertReturn(pStream->fWriteOrRead, VERR_ACCESS_DENIED);
    if (RT_FAILURE(pStream->rc))
        return pStream->rc;

    /*
     * Only deal with the simple cases here, use ScmStreamWrite for the
     * annoying stuff.
     */
    size_t off = pStream->off;
    if (   ch == '\n'
        || RT_UNLIKELY(off + 1 > pStream->cbAllocated))
        return ScmStreamWrite(pStream, &ch, 1);

    /*
     * Just append it.
     */
    pStream->pch[off] = ch;
    pStream->off = off + 1;
    pStream->paLines[pStream->iLine].cch++;

    return VINF_SUCCESS;
}

/**
 * Copies @a cLines from the @a pSrc stream onto the @a pDst stream.
 *
 * The stream positions will be used and changed in both streams.
 *
 * @returns IPRT status code.
 * @param   pDst                The destination stream.  Must be in write mode.
 * @param   cLines              The number of lines.  (0 is accepted.)
 * @param   pSrc                The source stream.  Must be in read mode.
 */
int ScmStreamCopyLines(PSCMSTREAM pDst, PSCMSTREAM pSrc, size_t cLines)
{
    AssertReturn(pDst->fWriteOrRead, VERR_ACCESS_DENIED);
    if (RT_FAILURE(pDst->rc))
        return pDst->rc;

    AssertReturn(!pSrc->fWriteOrRead, VERR_ACCESS_DENIED);
    if (RT_FAILURE(pSrc->rc))
        return pSrc->rc;

    while (cLines-- > 0)
    {
        SCMEOL      enmEol;
        size_t      cchLine;
        const char *pchLine = ScmStreamGetLine(pSrc, &cchLine, &enmEol);
        if (!pchLine)
            return pDst->rc = (RT_FAILURE(pSrc->rc) ? pSrc->rc : VERR_EOF);

        int rc = ScmStreamPutLine(pDst, pchLine, cchLine, enmEol);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}

