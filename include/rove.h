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

#define _GNU_SOURCE 1

#include <math.h>

#include <jack/jack.h>
#include <sndfile.h>
#include <monome.h>
#include <pthread.h>

#include "rove_file.h"
#include "rove_list.h"
#include "rove_pattern.h"

typedef struct rove_state {
	monome_t *monome;
	jack_client_t *client;
	
	uint8_t group_count;
	rove_group_t *groups;
	
	rove_list_t *files;
	rove_list_t *patterns;
	rove_list_member_t *pattern_rec;

	uint8_t staged_loops;
	rove_list_t *active;
	rove_list_t *staging;
	
	pthread_mutex_t monome_mutex;
	pthread_cond_t monome_display_notification;

	uint8_t active_loops;

	double bpm;
	double beat_multiplier;

	jack_nframes_t snap_delay;
	jack_nframes_t frames;
} rove_state_t;

#endif
