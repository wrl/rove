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

#ifndef _ROVE_TYPES_H
#define _ROVE_TYPES_H

#include <stdint.h>
#include <pthread.h>

#include <monome.h>
#include <jack/jack.h>
#include <sndfile.h>

#ifdef HAVE_SRC
#include <samplerate.h>
#endif

#include "list.h"

#define HANDLER_T(x) ((r_monome_handler_t *) x)
#define SESSION_T(x) ((session_t *) x)
#define PATTERN_T(x) ((pattern_t *) x)
#define PATTERN_STEP_T(x) ((pattern_step_t *) x)

/**
 * types
 */

typedef unsigned int uint_t;

typedef enum {
	FILE_STATUS_ACTIVE,
	FILE_STATUS_INACTIVE
} file_status_t;

typedef enum {
	FILE_PLAY_DIRECTION_FORWARD,
	FILE_PLAY_DIRECTION_REVERSE
} file_play_direction_t;

typedef enum {
	CMD_GROUP_DEACTIVATE,
	CMD_LOOP_SEEK
} pattern_cmd_t;

typedef enum {
	PATTERN_STATUS_RECORDING,
	PATTERN_STATUS_ACTIVE,
	PATTERN_STATUS_INACTIVE
} pattern_status_t;

typedef struct group group_t;
typedef struct file file_t;

typedef struct pattern pattern_t;
typedef struct pattern_step pattern_step_t;

typedef struct r_monome_handler r_monome_handler_t;
typedef struct r_monome_position r_monome_position_t;
typedef struct r_monome r_monome_t;

typedef struct session session_t;
typedef struct state state_t;

typedef void (*r_monome_callback_t)(r_monome_t *, uint_t x, uint_t y, uint_t event_type, void *user_arg);

typedef void (*process_callback_t)(file_t *self, jack_default_audio_sample_t **buffers, int channels, jack_nframes_t nframes, jack_nframes_t sample_rate);
typedef void (*quantize_callback_t)(file_t *self);
typedef void (*r_monome_output_callback_t)(file_t *self, r_monome_t *);

/**
 * r_monome
 */

struct r_monome_position {
	int x;
	int y;
};

struct r_monome_handler {
	r_monome_position_t pos;

	r_monome_callback_t cb;
	void *data;
};

struct r_monome {
	monome_t *dev;

	pthread_t thread;

	uint16_t quantize_field;
	uint16_t dirty_field;

	r_monome_handler_t *callbacks;
	r_monome_handler_t *controls;

	int mod_keys;
	int rows;
	int cols;
};

/**
 * file
 */

struct file {
	char *path;

	file_play_direction_t play_direction;
	file_status_t status;

	double volume;

	sf_count_t length;
	sf_count_t play_offset;
	sf_count_t new_offset;

	sf_count_t channels;
	sf_count_t file_length;
	sf_count_t sample_rate;

#ifdef HAVE_SRC	
	SRC_STATE *src;
#endif
	double speed;

	float *file_data;

	int y;
	int row_span;

	r_monome_t *mapped_monome;
	r_monome_position_t monome_pos;
	r_monome_position_t monome_pos_old;

	/* set to 1 if the next run of r_monome_display_file should
	   update the row regardless of whether it has changed. */
	int force_monome_update;

	unsigned int columns;

	group_t *group;

	process_callback_t process_cb;
	quantize_callback_t quantize_cb;
	r_monome_output_callback_t monome_out_cb;
	r_monome_callback_t monome_in_cb;
};

/**
 * group
 */

struct group {
	int idx;
	file_t *active_loop;

	double volume;

	/* eventually this will be an array of ports so that any arbitrary
	   number of channels can be output (rove cutting 5.1 audio, yeah!) */
	jack_port_t *outport_l;
	jack_port_t *outport_r;

	jack_default_audio_sample_t *output_buffer_l;
	jack_default_audio_sample_t *output_buffer_r;
};

/**
 * pattern
 */

struct pattern {
	list_member_t m;
	pattern_status_t status;
	r_monome_t *monome;

	list_t steps;
	pattern_step_t *current_step;

	int step_delay;
};

struct pattern_step {
	list_member_t m;

	file_t *victim;

	uint_t x;
	uint_t y;
	uint_t type;

	int delay;
};

/**
 * session
 */

struct session {
	list_member_t m;
	list_t files;

	char *path;
	char *dirname;

	uint_t cols;
	double bpm;
	double beat_multiplier;

	int pattern_lengths[2];
};

/**
 * rove
 */

struct state {
	struct {
		char *osc_prefix;
		char *osc_host_port;
		char *osc_listen_port;

		int cols;
		int rows;
	} config;

	r_monome_t *monome;
	jack_client_t *client;

	int group_count;
	group_t *groups;

	list_t sessions;
	session_t *active_session;

	list_t *files;
	list_t *patterns;
	pattern_t *pattern_rec;
	int *pattern_lengths;

	int staged_loops;

	double bpm;
	double beat_multiplier;

	jack_nframes_t snap_delay;
	jack_nframes_t frames_per_beat;
};

#endif
