/* $Id: d3d8_main.c 20227 2009-06-03 10:25:25Z vboxsync $ */

/** @file
 * VBox D3D8 dll switcher
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include "d3d8.h"
#include "switcher.h"

typedef HRESULT (WINAPI *D3D8GetSWInfoProc)(void);
typedef void (WINAPI *DebugSetMuteProc)(void);
typedef IDirect3D8* (WINAPI *Direct3DCreate8Proc)(UINT SDKVersion);
typedef HRESULT (WINAPI *ValidatePixelShaderProc)(DWORD* pixelshader, DWORD* reserved1, BOOL bool, DWORD* toto);
typedef HRESULT (WINAPI *ValidateVertexShaderProc)(DWORD* vertexshader, DWORD* reserved1, DWORD* reserved2, BOOL bool, DWORD* toto);

typedef struct _D3D8ExTag
{
    int                      initialized;
    const char              *vboxName;
    const char              *msName;
    D3D8GetSWInfoProc        pD3D8GetSWInfo;
    DebugSetMuteProc         pDebugSetMute;
    Direct3DCreate8Proc      pDirect3DCreate8;
    ValidatePixelShaderProc  pValidatePixelShader;
    ValidateVertexShaderProc pValidateVertexShader;
} D3D8Export;

static D3D8Export g_swd3d8 = {0, "VBoxD3D8.dll", "MSD3D8.dll",};

void FillD3DExports(HANDLE hDLL)
{
    SW_FILLPROC(g_swd3d8, hDLL, D3D8GetSWInfo);
    SW_FILLPROC(g_swd3d8, hDLL, DebugSetMute);
    SW_FILLPROC(g_swd3d8, hDLL, Direct3DCreate8);
    SW_FILLPROC(g_swd3d8, hDLL, ValidatePixelShader);
    SW_FILLPROC(g_swd3d8, hDLL, ValidateVertexShader);
}

HRESULT WINAPI D3D8GetSWInfo(void)
{
    SW_CHECKRET(g_swd3d8, D3D8GetSWInfo, E_FAIL);
    return g_swd3d8.pD3D8GetSWInfo();
}

void WINAPI DebugSetMute(void)
{
    SW_CHECKCALL(g_swd3d8, DebugSetMute);
    g_swd3d8.pDebugSetMute();    
}

IDirect3D8* WINAPI Direct3DCreate8(UINT SDKVersion)
{
    SW_CHECKRET(g_swd3d8, Direct3DCreate8, NULL);
    return g_swd3d8.pDirect3DCreate8(SDKVersion);
}

HRESULT WINAPI ValidatePixelShader(DWORD* pixelshader, DWORD* reserved1, BOOL bool, DWORD* toto)
{
    SW_CHECKRET(g_swd3d8, ValidatePixelShader, E_FAIL);
    return g_swd3d8.pValidatePixelShader(pixelshader, reserved1, bool, toto);
}

HRESULT WINAPI ValidateVertexShader(DWORD* vertexshader, DWORD* reserved1, DWORD* reserved2, BOOL bool, DWORD* toto)
{
    SW_CHECKRET(g_swd3d8, ValidateVertexShader, E_FAIL)
    return g_swd3d8.pValidateVertexShader(vertexshader, reserved1, reserved2, bool, toto);
}
