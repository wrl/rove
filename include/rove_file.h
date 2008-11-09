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

#include <samplerate.h>
#include <sndfile.h>
#include <stdint.h>

#include "rove_list.h"

#define rove_file_is_active(file) ( file->state == FILE_STATE_ACTIVE || file->state == FILE_STATE_RESEEK )

typedef enum {
	FILE_STATE_ACTIVE,
	FILE_STATE_INACTIVE,
	
	FILE_STATE_ACTIVATE,
	FILE_STATE_DEACTIVATE,
	FILE_STATE_RESEEK
} rove_file_state_t;

typedef enum {
	FILE_PLAY_DIRECTION_FORWARD,
	FILE_PLAY_DIRECTION_REVERSE
} rove_file_play_direction_t;


typedef struct rove_group {
	int idx;
	struct rove_file *active_loop;
} rove_group_t;

typedef struct rove_file {
	rove_file_play_direction_t play_direction;
	rove_file_state_t state;
	
	double volume;
	
	sf_count_t length;
	sf_count_t play_offset;
	sf_count_t loop_offset;
	
	sf_count_t channels;
	sf_count_t file_length;
	sf_count_t sample_rate;
	float *file_data;
	
	SRC_STATE* rate_conversion; /* someday */
	
	sf_count_t new_offset;
	
	uint8_t y;
	uint8_t row_span;
	
	uint8_t monome_pos;             /* position on the monome is represented as a 1-byte unsigned char*/
	uint8_t monome_pos_old;         /* the upper 4 bits (>> 4) are the X position */
	uint8_t force_monome_update;    /* and the lower 4 bits (& 0x0F) are the Y position */
	
	rove_group_t *group;
} rove_file_t;

rove_file_t*rove_file_new_from_path(const char *path);
void rove_file_free(rove_file_t*f);

sf_count_t rove_file_get_play_pos(rove_file_t *f);
void rove_file_inc_play_pos(rove_file_t *f, sf_count_t delta);
void rove_file_set_play_pos(rove_file_t *f, sf_count_t pos);

void rove_file_activate(rove_file_t *f);
void rove_file_deactivate(rove_file_t *f);
void rove_file_reseek(rove_file_t *f, sf_count_t seek_offset);

#endif
