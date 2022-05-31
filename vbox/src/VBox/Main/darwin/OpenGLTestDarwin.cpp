/* $Id: OpenGLTestDarwin.cpp 20162 2009-05-29 16:34:55Z vboxsync $ */

/** @file
 * VBox host opengl support test
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */


#include <OpenGL/OpenGL.h>
#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/gl.h>

bool is3DAccelerationSupported()
{
    CGDirectDisplayID   display = CGMainDisplayID ();
    CGOpenGLDisplayMask cglDisplayMask = CGDisplayIDToOpenGLDisplayMask (display);
    CGLPixelFormatObj   pixelFormat = NULL;
    GLint numPixelFormats = 0;

    CGLPixelFormatAttribute attribs[] = {
        kCGLPFADisplayMask,
        (CGLPixelFormatAttribute)cglDisplayMask,
        kCGLPFAAccelerated,
        kCGLPFADoubleBuffer,
        kCGLPFAWindow,
        (CGLPixelFormatAttribute)NULL
    };

    display = CGMainDisplayID();
    cglDisplayMask = CGDisplayIDToOpenGLDisplayMask(display);
    CGLChoosePixelFormat(attribs, &pixelFormat, &numPixelFormats);

    if (pixelFormat)
    {
        CGLContextObj cglContext = 0;
        CGLCreateContext(pixelFormat, NULL, &cglContext);
        CGLDestroyPixelFormat(pixelFormat);
        if (cglContext)
        {
            CGLDestroyContext(cglContext);
            return true;
        }
    }

    return false;
}

