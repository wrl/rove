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

#include <jack/jack.h>

#include "rove_file.h"
#include "rove_list.h"

typedef enum {
	CMD_GROUP_DEACTIVATE,
	CMD_LOOP_SEEK
} rove_pattern_cmd_t;

typedef enum {
	PATTERN_STATUS_RECORDING,
	PATTERN_STATUS_ACTIVE,
	PATTERN_STATUS_ACTIVATE,
	PATTERN_STATUS_INACTIVE
} rove_pattern_status_t;

typedef struct rove_pattern_step {
	jack_nframes_t delay;
	rove_pattern_cmd_t cmd;
	rove_file_t *file;
	jack_nframes_t arg;
} rove_pattern_step_t;

typedef struct rove_pattern {
	rove_list_t *steps;
	rove_list_member_t *current_step;
	
	jack_nframes_t delay_frames;
	rove_pattern_status_t status;
} rove_pattern_t;

rove_pattern_t *rove_pattern_new();
void rove_pattern_free(rove_pattern_t *);

void rove_pattern_append_step(rove_pattern_t *, rove_pattern_cmd_t, rove_file_t *, jack_nframes_t);
	
#endif
