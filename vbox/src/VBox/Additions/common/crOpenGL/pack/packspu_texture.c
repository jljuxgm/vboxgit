/* $Id: packspu_texture.c 20084 2009-05-27 13:26:41Z vboxsync $ */

/** @file
 * VBox OpenGL DRI driver functions
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include "packspu.h"
#include "cr_packfunctions.h"
#include "cr_glstate.h"
#include "packspu_proto.h"

void PACKSPU_APIENTRY packspu_ActiveTextureARB(GLenum texture)
{
    crStateActiveTextureARB(texture);
    crPackActiveTextureARB(texture);
}
