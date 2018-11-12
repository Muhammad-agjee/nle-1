/* NetHack 3.6	winos.h	$NHDT-Date: 1524321419 2018/04/21 14:36:59 $  $NHDT-Branch: NetHack-3.6.0 $:$NHDT-Revision: 1.30 $ */
/* Copyright (c) NetHack PC Development Team 2018 */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef WINOS_H
#define WINOS_H

#include "win32api.h"

extern const WCHAR cp437[256];

WCHAR *
winos_ascii_to_wide_str(const unsigned char * src, WCHAR * dst, size_t dstLength);

WCHAR
winos_ascii_to_wide(const unsigned char c);

BOOL winos_font_support_cp437(HFONT hFont);

#endif // WINOS_H
