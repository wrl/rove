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

/**
 * types
 */

typedef enum {
	FILE_STATUS_ACTIVE,
	FILE_STATUS_INACTIVE
} rove_file_status_t;

typedef enum {
	FILE_PLAY_DIRECTION_FORWARD,
	FILE_PLAY_DIRECTION_REVERSE
} rove_file_play_direction_t;

typedef enum {
	HEAD,
	TAIL
} rove_list_global_location_t;

typedef enum {
	BEFORE,
	AFTER
} rove_list_local_location_t;

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

typedef enum {
	INT,
	LONG,
	STRING,
	DOUBLE,
	BOOL
} rove_config_var_type_t;

typedef struct rove_group rove_group_t;
typedef struct rove_file rove_file_t;

typedef struct rove_list_member rove_list_member_t;
typedef struct rove_list rove_list_t;

typedef struct rove_monome_handler rove_monome_handler_t;
typedef struct rove_monome_position rove_monome_position_t;
typedef struct rove_monome rove_monome_t;

typedef struct rove_pattern_step rove_pattern_step_t;
typedef struct rove_pattern rove_pattern_t;

typedef struct rove_state rove_state_t;

typedef struct rove_config_pair rove_config_pair_t;
typedef struct rove_config_section rove_config_section_t;
typedef struct rove_config_var rove_config_var_t;


typedef void (*rove_monome_callback_t)(rove_monome_handler_t *self, rove_state_t *, rove_monome_t *, const uint8_t x, const uint8_t y, const uint8_t event_type);
typedef void (*rove_config_section_callback_t)(const rove_config_section_t *, void *arg);

typedef void (*rove_process_callback_t)(rove_file_t *self, jack_default_audio_sample_t **buffers, int channels, jack_nframes_t nframes);
typedef void (*rove_quantize_callback_t)(rove_file_t *self);
typedef void (*rove_monome_input_callback_t)(rove_file_t *self, rove_monome_t *, const int x, const int y, const int event_type);
typedef void (*rove_monome_output_callback_t)(rove_file_t *self, rove_monome_t *);

/**
 * rove_config
 */

struct rove_config_var {
	const char *key;
	void *dest;
	rove_config_var_type_t type;
	int val;
};

struct rove_config_pair {
	char *key;
	char *value;
	
	const rove_config_var_t *var;
	int klen;
	int vlen;
};

struct rove_config_section {
	char *block;
	const rove_config_var_t *vars;
	rove_config_section_callback_t section_callback;
	void *cb_arg;

	int start_line;
	rove_list_t *pairs;
};

/**
 * rove_monome
 */

struct rove_monome_position {
	uint8_t x;
	uint8_t y;
};

struct rove_monome_handler {
	rove_monome_position_t pos;
	
	rove_monome_callback_t cb;
	void *data;
};

struct rove_monome {
	monome_t *dev;

	pthread_t thread;

	uint16_t quantize_field;
	uint16_t dirty_field;
	
	rove_monome_handler_t *callbacks;
	rove_monome_handler_t *controls;
	
	uint8_t mod_keys;
	uint8_t rows;
	uint8_t cols;
};

/**
 * rove_file
 */

struct rove_file {
	rove_file_play_direction_t play_direction;
	rove_file_status_t status;
	
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
	
	uint8_t y;
	uint8_t row_span;
	
	rove_monome_t *mapped_monome;
	rove_monome_position_t monome_pos;
	rove_monome_position_t monome_pos_old;
	
	/* set to 1 if the next run of rove_monome_display_file should
	   update the row regardless of whether it has changed. */
	uint8_t force_monome_update;
	
	unsigned int columns;
	
	rove_group_t *group;
	
	rove_process_callback_t process;
	rove_quantize_callback_t quantize_callback;
};

/**
 * rove_group
 */

struct rove_group {
	int idx;
	rove_file_t *active_loop;
	
	double volume;
	
	/* eventually this will be an array of ports so that any arbitrary
	   number of channels can be output (rove cutting 5.1 audio, yeah!) */
	jack_port_t *outport_l;
	jack_port_t *outport_r;
	
	jack_default_audio_sample_t *output_buffer_l;
	jack_default_audio_sample_t *output_buffer_r;
};

/**
 * rove_list
 */

struct rove_list_member {
	void *data;
	
	rove_list_member_t *prev;
	rove_list_member_t *next;
};

struct rove_list {
	rove_list_member_t *head;
	rove_list_member_t *tail;
};

/**
 * rove_pattern
 */

struct rove_pattern_step {
	int delay;
	rove_pattern_cmd_t cmd;
	rove_file_t *file;
	jack_nframes_t arg;
};

struct rove_pattern {
	rove_list_t *steps;
	rove_list_member_t *current_step;
	
	int delay_steps;
	rove_pattern_status_t status;
	
	rove_monome_handler_t *bound_button;
};

/**
 * rove
 */

struct rove_state {
	rove_monome_t *monome;
	jack_client_t *client;
	
	uint8_t group_count;
	rove_group_t *groups;
	
	rove_list_t *files;
	rove_list_t *patterns;
	rove_list_member_t *pattern_rec;
	uint8_t pattern_lengths[2];

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
};

#endif
