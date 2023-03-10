/**
 * @file util.c
 * @brief set of utility functions
 * @author Gilbert Brault
 * @copyright Gilbert Brault 2015
 * the original work comes from bluez v5.39
 * value add: documenting main features
 *
 */
/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2012-2014  Intel Corporation. All rights reserved.
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#define _GNU_SOURCE
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>

#include "util.h"

/**
 * create a str debug message using format and then call function(str,user_data)
 *
 * @param function	function to call
 * @param user_data	data for the "function"
 * @param format	format string template of str string
 */
void util_debug(util_debug_func_t function, void * user_data,
                const char * format, ...) {
    char str[78];
    va_list ap;

    if (!function || !format)
        return;

    va_start(ap, format);
    vsnprintf(str, sizeof(str), format, ap);
    va_end(ap);

    function(str, user_data);
}

/**
 * hexadecimal dump utility: create the str hex string and then call function(str,user_data)
 *
 * @param dir			first char of str
 * @param buf			buffer to convert to hex (str)
 * @param len			size of buffer (should be less than or equal to 16)
 * @param function		function to call with (str,user_data)
 * @param user_data		pointer to pass to function
 */
void util_hexdump(const char dir, const unsigned char * buf, size_t len,
                  util_debug_func_t function, void * user_data) {
    static const char hexdigits[] = "0123456789abcdef";
    char str[68];
    size_t i;

    if (!function || !len)
        return;

    str[0] = dir;

    for (i = 0; i < len; i++) {
        str[((i % 16) * 3) + 1] = ' ';
        str[((i % 16) * 3) + 2] = hexdigits[buf[i] >> 4];
        str[((i % 16) * 3) + 3] = hexdigits[buf[i] & 0xf];
        str[(i % 16) + 51] = isprint(buf[i]) ? buf[i] : '.';

        if ((i + 1) % 16 == 0) {
            str[49] = ' ';
            str[50] = ' ';
            str[67] = '\0';
            function(str, user_data);
            str[0] = ' ';
        }
    }

    if (i % 16 > 0) {
        size_t j;
        for (j = (i % 16); j < 16; j++) {
            str[(j * 3) + 1] = ' ';
            str[(j * 3) + 2] = ' ';
            str[(j * 3) + 3] = ' ';
            str[j + 51] = ' ';
        }
        str[49] = ' ';
        str[50] = ' ';
        str[67] = '\0';
        function(str, user_data);
    }
}

/**
 * Helper for getting the dirent type in case readdir
 *
 * @param parent	prefix to build full path
 * @param name		suffix to builld full path
 * @return DT_UNKNOWN or DT_DIR if parent/name is a DIR
 */
unsigned char util_get_dt(const char * parent, const char * name) {
    char filename[PATH_MAX];
    struct stat st;

    snprintf(filename, sizeof(filename), "%s/%s", parent, name);
    if (lstat(filename, &st) == 0 && S_ISDIR(st.st_mode))
        return DT_DIR;

    return DT_UNKNOWN;
}

/**
 * Helpers for bitfield operations
 *
 * Find unique id in range from 1 to max but no bigger then
 * sizeof(int) * 8. ffs() is used since it is POSIX standard
 *
 * @param bitmap	map of bits
 * @param max		max bit position
 * @return
 */
uint8_t util_get_uid(unsigned int * bitmap, uint8_t max) {
    uint8_t id;

    id = ffs(~*bitmap);

    if (!id || id > max)
        return 0;

    *bitmap |= 1 << (id - 1);

    return id;
}

/**
 * Clear id bit in bitmap
 *
 * @param bitmap	map of bits
 * @param id		bit rank to clear
 */
void util_clear_uid(unsigned int * bitmap, uint8_t id) {
    if (!id)
        return;

    *bitmap &= ~(1 << (id - 1));
}
