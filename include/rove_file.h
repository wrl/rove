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

#ifndef _ROVE_FILE_H
#define _ROVE_FILE_H

#include <sndfile.h>
#include <stdint.h>

#include "rove_types.h"

#define rove_file_is_active(f) ( f->state == FILE_STATE_ACTIVE || f->state == FILE_STATE_RESEEK )
#define rove_file_get_play_pos(f) (f->play_offset * f->channels)

rove_file_t*rove_file_new_from_path(const char *path);
void rove_file_free(rove_file_t*f);

void rove_file_set_play_pos(rove_file_t *f, sf_count_t pos);
void rove_file_inc_play_pos(rove_file_t *f, sf_count_t delta);

void rove_file_activate(rove_file_t *f);
void rove_file_deactivate(rove_file_t *f);
void rove_file_reseek(rove_file_t *f, jack_nframes_t offset);

#endif
