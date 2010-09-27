/**
 * This file is part of rove.
 * rove is copyright 2007, 2008 william light <visinin@gmail.com>
 *
 * rove is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rove is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rove.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ROVE_H
#define _ROVE_H

#include <math.h>

#include "types.h"
#include "file.h"
#include "list.h"
#include "pattern.h"

#define usage_printf_exit(...)		do { usage(); printf(__VA_ARGS__); exit(EXIT_FAILURE); } while(0);
#define usage_printf_return(...)	do { usage(); printf(__VA_ARGS__); return 1;           } while(0);

void usage();
int is_numstr(char *str);

#endif
