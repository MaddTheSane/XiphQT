/*
 *  pxml.c
 *
 *    Very simple xml plist file parser.
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


#include "pxml.h"

#include <string.h>

CFStringRef pxml_parse_key(unsigned char **str, long *str_size) {
    CFStringRef ret = NULL;

    char *l_str_pos = *str;
    char *tmp_pos = NULL;
    char *end_pos = NULL;
    long l_str_size = *str_size;

    if (strncmp(l_str_pos, "<key", 4) != 0)
        return NULL;
    else {
        l_str_pos += 4;
        l_str_size -= 4;
    }

    tmp_pos = memchr(l_str_pos, '>', l_str_size);
    if (tmp_pos == NULL)
        return NULL;

    tmp_pos += 1;
    l_str_size -= tmp_pos - l_str_pos;
    l_str_pos = tmp_pos;
    if (l_str_size < 0)
        return NULL;

    end_pos = memchr(l_str_pos, '<', l_str_size);
    if (end_pos == NULL)
        return NULL;

    l_str_size -= end_pos - l_str_pos;
    l_str_pos = end_pos;

    if (l_str_size < 6 || strncmp(l_str_pos, "</key>", 6) != 0)
        return NULL;

    *str = end_pos + 6;
    *str_size = l_str_size - 6;

    ret = CFStringCreateWithBytes(NULL, tmp_pos, end_pos - tmp_pos, kCFStringEncodingUTF8, true);

    return ret;
}

CFStringRef pxml_parse_string(unsigned char **str, long *str_size) {
    CFStringRef ret = NULL;

    char *l_str_pos = *str;
    char *tmp_pos = NULL;
    char *end_pos = NULL;
    long l_str_size = *str_size;

    if (strncmp(l_str_pos, "<string", 7) != 0)
        return NULL;
    else {
        l_str_pos += 7;
        l_str_size -= 7;
    }

    tmp_pos = memchr(l_str_pos, '>', l_str_size);
    if (tmp_pos == NULL)
        return NULL;

    tmp_pos += 1;
    l_str_size -= tmp_pos - l_str_pos;
    l_str_pos = tmp_pos;
    if (l_str_size < 0)
        return NULL;

    end_pos = memchr(l_str_pos, '<', l_str_size);
    if (end_pos == NULL)
        return NULL;

    l_str_size -= end_pos - l_str_pos;
    l_str_pos = end_pos;

    if (l_str_size < 9 || strncmp(l_str_pos, "</string>", 9) != 0)
        return NULL;

    *str = end_pos + 9;
    *str_size = l_str_size - 9;

    ret = CFStringCreateWithBytes(NULL, tmp_pos, end_pos - tmp_pos, kCFStringEncodingUTF8, true);

    return ret;
}

CFDictionaryRef pxml_parse_dict(unsigned char **str, long *str_size) {
    CFDictionaryRef ret = NULL;
    CFMutableDictionaryRef tmp_ret = NULL;
    char *l_str_pos = *str;
    char *tmp_pos = NULL;
    long l_str_size = *str_size;

    if (strncmp(l_str_pos, "<dict", 5) != 0)
        return NULL;
    else {
        l_str_pos += 5;
        l_str_size -= 5;
    }

    tmp_ret = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    while ((tmp_pos = memchr(l_str_pos, '<', l_str_size)) != NULL) {
        CFStringRef d_key = NULL;
        void *d_value = NULL;

        l_str_size -= tmp_pos - l_str_pos;
        l_str_pos = tmp_pos;

        if (l_str_size > 7 && strncmp(l_str_pos, "</dict>", 7) == 0) {
            l_str_size -= 7;
            l_str_pos += 7;
            break;
        } else if (l_str_size < 4 || strncmp(l_str_pos, "<key", 4) != 0)
            break;

        d_key = pxml_parse_key(&l_str_pos, &l_str_size);
        if (d_key == NULL)
            break;

        tmp_pos = memchr(l_str_pos, '<', l_str_size);
        if (tmp_pos == NULL) {
            CFRelease(d_key);
            break;
        }

        l_str_size -= tmp_pos - l_str_pos;
        l_str_pos = tmp_pos;

        if (l_str_size > 7 && strncmp(l_str_pos, "</dict>", 7) == 0) {
            l_str_size -= 7;
            l_str_pos += 7;
            break;
        } else if (l_str_size > 7 && strncmp(l_str_pos, "<string", 7) == 0) {
            d_value = pxml_parse_string(&l_str_pos, &l_str_size);
        } else if (l_str_size > 5 && strncmp(l_str_pos, "<dict", 5) == 0) {
            d_value = pxml_parse_dict(&l_str_pos, &l_str_size);
        } else {
            // other value types not supported
            CFRelease(d_key);
            break;
        }

        if (d_value == NULL) {
            CFRelease(d_key);
            break;
        }
        CFDictionaryAddValue(tmp_ret, d_key, d_value);
    }


    if (CFDictionaryGetCount(tmp_ret) > 0) {
        ret = CFDictionaryCreateCopy(NULL, tmp_ret);
        CFDictionaryRemoveAllValues(tmp_ret);
    }

    if (ret != NULL) {
        *str = l_str_pos;
        *str_size = l_str_size;
    }

    return ret;
}


CFDictionaryRef pxml_parse_plist(unsigned char *plist_str, long plist_size) {
    CFDictionaryRef ret = NULL;
    char *l_str_pos = plist_str;
    char *tmp_pos = NULL;
    long l_str_size = plist_size;

    while (l_str_size > 6 && strncmp(l_str_pos, "<plist", 6) != 0) {
        tmp_pos = memchr(l_str_pos + 1, '<', l_str_size - 1);

        if (tmp_pos == NULL) {
            l_str_pos = NULL;
            break;
        }

        l_str_size -= tmp_pos - l_str_pos;
        l_str_pos = tmp_pos;
    }

    if (l_str_pos != NULL) {
        l_str_pos += 6;
        l_str_size -= 6;

        while (l_str_size > 5 && strncmp(l_str_pos, "<dict", 5) != 0) {
            tmp_pos = memchr(l_str_pos + 1, '<', l_str_size - 1);

            if (tmp_pos == NULL) {
                l_str_pos = NULL;
                break;
            }

            l_str_size -= tmp_pos - l_str_pos;
            l_str_pos = tmp_pos;
        }

        if (l_str_pos != NULL) {
            ret = pxml_parse_dict(&l_str_pos, &l_str_size);
        }
    }

    return ret;
}


#if defined(PXML_TEST_IN_PLACE)

#include <stdio.h>
#include <fcntl.h>

int main(int argc, char **argv) {
    char fbuf[65536];
    CFDictionaryRef dict = NULL;
    int f = open(argv[1], O_RDONLY, 0);
    int bytes_read = read(f, fbuf, 65535);
    close(f);
    fbuf[bytes_read] = '\0';
    dict = parse_plist(fbuf, strlen(fbuf));

    if (dict != NULL)
        printf("Key count: %d\n", CFDictionaryGetCount(dict));

    return 0;
}
#endif /* PXML_TEST_IN_PLACE */
