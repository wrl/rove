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

#include <sndfile.h>
#include <stdlib.h>

#include <lo/lo.h>

#include "rove.h"
#include "rove_file.h"
#include "rove_jack.h"
#include "rove_list.h"
#include "rove_pattern.h"

#define MONOME_COLS     16

#define BUTTON_DOWN     1
#define BUTTON_UP       0

#define CLEAR_OFF       0

#define OSC_PREFIX      "rove"
#define OSC_HOST_PORT   "8080"
#define OSC_LISTEN_PORT "8000"

#define SHIFT 0x01
#define META  0x02

#define BEATS_IN_PATTERN 8

/**
 * (mostly) emulate libmonome functions in liblo
 */

void monome_led_row_16(rove_monome_t *monome, uint8_t row, uint8_t *row_data) {
	char *buf;
	
	asprintf(&buf, "/%s/led_row", OSC_PREFIX);
	lo_send_from(monome->outgoing, lo_server_thread_get_server(monome->st), LO_TT_IMMEDIATE, buf, "iii", row, row_data[0], row_data[1]);
	free(buf);

	return;
}

void monome_clear(rove_monome_t *monome, uint8_t mode) {
	char *buf;
	
	asprintf(&buf, "/%s/clear", OSC_PREFIX);
	lo_send_from(monome->outgoing, lo_server_thread_get_server(monome->st), LO_TT_IMMEDIATE, buf, "i", mode);
	free(buf);
}

void monome_led_on(rove_monome_t *monome, uint8_t x, uint8_t y) {
	char *buf;
	
	asprintf(&buf, "/%s/led", OSC_PREFIX);
	lo_send_from(monome->outgoing, lo_server_thread_get_server(monome->st), LO_TT_IMMEDIATE, buf, "iii", x, y, 1);
	free(buf);
}

void monome_led_off(rove_monome_t *monome, uint8_t x, uint8_t y) {
	char *buf;
	
	asprintf(&buf, "/%s/led", OSC_PREFIX);
	lo_send_from(monome->outgoing, lo_server_thread_get_server(monome->st), LO_TT_IMMEDIATE, buf, "iii", x, y, 0);
	free(buf);
}

/**
 */

static void lo_error(int num, const char *error_msg, const char *path) {
	printf("rove_monome: liblo server error %d in %s: %s\n", num, path, error_msg);
	fflush(stdout);
}

static uint8_t calculate_monome_pos(sf_count_t length, sf_count_t position, uint8_t rows) {
	double elapsed;
	uint8_t x, y;
	
	elapsed = position / (double) length;
	x = lrint(floor(elapsed * (((double) 16) * rows)));
	y = (x / 16) & 0x0F;
	
	return ((x << 4) | y);
}

static sf_count_t calculate_play_pos(sf_count_t length, uint8_t position_x, uint8_t position_y, uint8_t rows, uint8_t reverse, sf_count_t channels) {
	double elapsed;
	uint8_t x;
	
	position_x &= 0x0F;

	if( reverse )
		position_x += 1;
	
	x = position_x + ((position_y) * 16);
	elapsed = x / (double) (16 * rows);
	
	if( reverse )
		return lrint(floor(elapsed * length));
	else
		return lrint(ceil(elapsed * length));
}

static void blank_file_row(rove_monome_t *monome, rove_file_t *f) {
	uint8_t row[2] = {0, 0};
	monome_led_row_16(monome, f->y + (f->monome_pos & 0x0F), row);
}

static void *pattern_post_record(rove_state_t *state, rove_monome_t *monome, const uint8_t x, const uint8_t y, rove_list_member_t *m) {
	rove_pattern_t *p = m->data;

	state->pattern_rec = NULL;
			
	if( rove_list_is_empty(p->steps) ) {
		p->status = PATTERN_STATUS_INACTIVE;

		rove_list_remove(state->patterns, m);
		rove_pattern_free(p);
				
		monome_led_off(monome, p->idx + state->group_count, y);
		return NULL;
	}
	
	return m;
}

static void *pattern_handler(rove_state_t *state, const uint8_t x, const uint8_t y, const uint8_t mod_keys, void *arg) {
	rove_monome_t *monome = state->monome;

	rove_list_member_t *m = arg;
	rove_pattern_t *p;
	
	if( !m ) {
		if( state->pattern_rec ) {
			p = state->pattern_rec->data;
			if( (monome->callbacks[p->idx + state->group_count].arg = pattern_post_record(state, monome, x, y, state->pattern_rec)) )
				p->status = PATTERN_STATUS_ACTIVATE;
		}
		
		p = rove_pattern_new();
		p->idx = x - state->group_count;
		p->status = PATTERN_STATUS_RECORDING;
		p->delay_frames = (lrint(1 / state->beat_multiplier) * state->pattern_lengths[p->idx]) * state->snap_delay;
		
		m = rove_list_push(state->patterns, TAIL, p);
		state->pattern_rec = m;
		
		monome_led_on(monome, x, y);
	} else {
		p = m->data;
		
		if( mod_keys & SHIFT ) {
			if( state->pattern_rec == m )
				state->pattern_rec = NULL;
			
			p->status = PATTERN_STATUS_INACTIVE;

			while( !rove_list_is_empty(p->steps) )
				free(rove_list_pop(p->steps, HEAD));
			
			rove_pattern_free(m->data);
			rove_list_remove(state->patterns, m);
			monome_led_off(monome, x, y);
			return NULL;
		}

		switch( p->status ) {
		case PATTERN_STATUS_RECORDING:
			if( state->pattern_rec != m ) {
				printf("what: pattern is marked as recording but is not the globally-designated recordee\n"
					   "      this shouldn't happen.  visinin fucked up, sorry :(\n");
				return NULL;
			}
			
			if( !pattern_post_record(state, monome, x, y, m) )
				return NULL;
			
			/* fall through */

		case PATTERN_STATUS_ACTIVATE:
		case PATTERN_STATUS_INACTIVE:
			p->status = PATTERN_STATUS_ACTIVATE;
			monome_led_on(monome, x, y);
			break;
			
		case PATTERN_STATUS_ACTIVE:
			p->status = PATTERN_STATUS_INACTIVE;
			monome_led_off(monome, x, y);
			break;
		}
	}
	
	return m;
}

static void *group_off_handler(rove_state_t *state, const uint8_t x, const uint8_t y, const uint8_t mod_keys, void *arg) {
	rove_group_t *group = arg;
	rove_file_t *f;

	if( x > (state->group_count - 1) )
		return arg;
				
	if( !(f = group->active_loop) )
		return arg;
				
	if( !rove_file_is_active(f) )
		return arg;
				
	if( state->pattern_rec )
		rove_pattern_append_step(state->pattern_rec->data, CMD_GROUP_DEACTIVATE, f, 0);
				
	f->state = FILE_STATE_DEACTIVATE;
	return arg;
}

static int button_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message data, void *user_data) {
	static uint8_t mod_keys = 0;

	rove_state_t *state = user_data;
	rove_monome_t *monome = state->monome;
	rove_monome_callback_t *callback;

	int event_x, event_y, event_type;
	
	rove_file_t *f;
	rove_list_member_t *m;

	event_x    = argv[0]->i;
	event_y    = argv[1]->i;
	event_type = argv[2]->i;

	switch( event_type ) {
	case BUTTON_DOWN:
		if( event_y < 1 ) {
			callback = &monome->callbacks[event_x];
			
			if( callback->cb ) {
				callback->arg = callback->cb(state, event_x, event_y, mod_keys, callback->arg);
			} else {
				if( event_x == state->group_count + 2 )
					mod_keys |= SHIFT;
				else if( event_x == state->group_count + 3 )
					mod_keys |= META;
			}
		} else {
			rove_list_foreach(state->files, m, f) {
				if( event_y < f->y || event_y > ( f->y + f->row_span - 1) )
					continue;
		
				f->new_offset = calculate_play_pos(f->file_length, event_x, (event_y - f->y),
												   f->row_span, (f->play_direction == FILE_PLAY_DIRECTION_REVERSE), f->channels);
				
				if( state->pattern_rec )
					rove_pattern_append_step(state->pattern_rec->data, CMD_LOOP_SEEK, f, f->new_offset);
				
				if( !rove_file_is_active(f) ) {
					f->force_monome_update = 1;
					f->state = FILE_STATE_ACTIVATE;
				
					f->play_offset = f->new_offset;
					f->new_offset  = -1;
				} else {
					f->state = FILE_STATE_RESEEK;
				}
			}
		}
		
		break;
		
	case BUTTON_UP:
		if( event_y < 1 ) {
			if( event_x == state->group_count + 2 )
				mod_keys &= ~SHIFT;
			else if( event_x == state->group_count + 3 )
				mod_keys &= ~META;
		}
		
		break;
	}

	return 0;
}

void rove_monome_blank_file_row(rove_state_t *state, rove_file_t *f) {
	blank_file_row(state->monome, f);
}

void rove_monome_display_file(rove_state_t *state, rove_file_t *f) {
	rove_monome_t *monome = state->monome;
	uint8_t row[2], pos;
	uint16_t r;
	
	pos = calculate_monome_pos(f->file_length * f->channels, rove_file_get_play_pos(f), f->row_span);
	
	if( pos != f->monome_pos_old || f->force_monome_update ) {
		if( f->force_monome_update ) {
			monome_led_on(monome, f->group->idx, 0);
			f->force_monome_update = 0;
		}

		if( (pos & 0x0F) != (f->monome_pos_old & 0x0F) ) {
			row[0] = row[1] = 0;
			monome_led_row_16(monome, f->y + (f->monome_pos_old & 0x0F), row);
		}
		
		f->monome_pos_old = pos;
	
		r      = 1 << ( pos >> 4 );
		row[0] = r & 0x00FF;
		row[1] = r >> 8;
		monome_led_row_16(monome, f->y + (pos & 0x0F), row);
	}

	f->monome_pos = pos;
}

void rove_monome_run_thread(rove_state_t *state) {
	lo_server_thread_start(state->monome->st);
}

int rove_monome_init(rove_state_t *state) {
	rove_monome_t *monome = calloc(sizeof(rove_monome_t), 1);
	char *buf;
	int i;
	
	if( !(monome->st = lo_server_thread_new(OSC_LISTEN_PORT, lo_error)) ) {
		free(monome);
		return -1;
	}

	asprintf(&buf, "/%s/press", OSC_PREFIX);
	lo_server_thread_add_method(monome->st, buf, "iii", button_handler, state);
	free(buf);
	
	monome->outgoing  = lo_address_new(NULL, OSC_HOST_PORT);
	monome->callbacks = calloc(sizeof(rove_monome_callback_t), MONOME_COLS);
	
	for( i = 0; i < state->group_count; i++ ) {
		monome->callbacks[i].cb  = group_off_handler;
		monome->callbacks[i].arg = (void *) &state->groups[i];
	}
	
	for( ; i < state->group_count + 2; i++ ) {
		monome->callbacks[i].cb  = pattern_handler;
		monome->callbacks[i].arg = NULL;
	}
	
	monome_clear(monome, CLEAR_OFF);

	state->monome = monome;
	return 0;
}
