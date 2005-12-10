/*
 *  pxml.h
 *
 *    Very simple xml plist file parser - header file.
 *
 *
 *  Copyright (c) 2005  Arek Korbik
 *
 *  This file is part of XiphQT, the Xiph QuickTime Components.
 *
 *  XiphQT is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  XiphQT is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with XiphQT; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 *  Last modified: $Id$
 *
 */


#if !defined(__pxml_h__)
#define __pxml_h__

#if defined(__APPLE_CC__)
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#else
#include <CoreServices.h>
#include <CoreFoundation.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif


extern CFDictionaryRef pxml_parse_plist(unsigned char *plist_str, long plist_size);


#ifdef __cplusplus
}
#endif


#endif /* __pxml_h__ */
