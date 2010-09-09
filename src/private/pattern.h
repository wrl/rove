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

#ifndef _ROVE_PATTERN_H
#define _ROVE_PATTERN_H

#include "types.h"

void pattern_record(r_monome_callback_t cb, void *victim, uint_t x, uint_t y, uint_t type);

void pattern_status_set(pattern_t *self, pattern_status_t nstatus);
void pattern_process(pattern_t *self);

pattern_t *pattern_new();
void pattern_free(pattern_t *);

#endif
