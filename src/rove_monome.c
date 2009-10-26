/**
 * This file is part of rove.
 * rove is copyright 2007-2009 william light <visinin@gmail.com>
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

#include <assert.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <monome.h>
#include <sndfile.h>

#include "rove.h"
#include "rove_file.h"
#include "rove_jack.h"
#include "rove_list.h"
#include "rove_util.h"
#include "rove_monome.h"
#include "rove_pattern.h"

#define SHIFT 0x01
#define META  0x02

#define MONOME_POS_CMP(a, b) (memcmp(a, b, sizeof(rove_monome_position_t)))
#define MONOME_POS_CPY(a, b) (memcpy(a, b, sizeof(rove_monome_position_t)))

static void calculate_monome_pos(sf_count_t length, sf_count_t position, uint8_t rows, uint8_t cols, rove_monome_position_t *pos) {
	double elapsed;
	uint8_t x, y;
	
	elapsed = position / (double) length;
	x  = lrint(floor(elapsed * (((double) cols) * rows)));
	y  = (x / cols) & 0x0F;
	x %= cols;

	pos->x = x;
	pos->y = y;
}

static sf_count_t calculate_play_pos(sf_count_t length, uint8_t x, uint8_t y, uint8_t reverse, uint8_t rows, uint8_t cols) {
	double elapsed;
	
	x &= 0x0F;

	if( reverse )
		x += 1;
	
	x += y * cols;
	elapsed = x / (double) (cols * rows);
	
	if( reverse )
		return lrint(floor(elapsed * length));
	else
		return lrint(ceil(elapsed * length));
}

static void *pattern_post_record(rove_state_t *state, rove_monome_t *monome, const uint8_t x, const uint8_t y, rove_list_member_t *m) {
	rove_pattern_t *p = m->data;

	state->pattern_rec = NULL;
	
	/* deallocate pattern if it contains no steps */
	if( rove_list_is_empty(p->steps) ) {
		/* first mark it as inactive so that rove_jack.process() skips over it */
		p->status = PATTERN_STATUS_INACTIVE;

		if( p->bound_button ) {
			monome_led_off(monome->dev, p->bound_button->pos.x, p->bound_button->pos.y);
			p->bound_button->data = NULL;
		}

		rove_list_remove(state->patterns, m);
		rove_pattern_free(p);
				
		return NULL;
	}
	
	return p;
}

static void pattern_handler(rove_monome_handler_t *self, rove_state_t *state, rove_monome_t *monome, const uint8_t x, const uint8_t y, const uint8_t event_type) {
	rove_list_member_t *m = self->data;
	rove_pattern_t *p;
	int idx;

	if( event_type != MONOME_BUTTON_DOWN )
		return;
	
	/* pattern allocation (i.e. no pattern currently associated with this button) */
	if( !m ) {
		/* if there is a pattern currently being recorded to, finalize it before allocating a new one */
		if( state->pattern_rec )
			if( (p = pattern_post_record(state, monome, x, y, state->pattern_rec)) )
				p->status = PATTERN_STATUS_ACTIVATE;
		
		idx = x - (monome->cols - 4);
		
		p = rove_pattern_new();
		p->status = PATTERN_STATUS_RECORDING;
		p->delay_steps = (lrint(1 / state->beat_multiplier) * state->pattern_lengths[idx]);
		
		m = rove_list_push(state->patterns, TAIL, p);
		state->pattern_rec = m;
		self->data = m;
		
		p->bound_button = self;
		
		monome_led_on(monome->dev, x, y);

		self->data = m;
		return;
	}

	p = m->data;
	
	/* pattern erasure/deletion (pressing in conjunction with shift) */
	if( monome->mod_keys & SHIFT ) {
		/* if this pattern is currently set as the global recordee, remove it */
		if( state->pattern_rec == m )
			state->pattern_rec = NULL;
			
		/* set as inactive so that rock_jack.process() skips over it */
		p->status = PATTERN_STATUS_INACTIVE;

		/* deallocate all the steps */
		while( !rove_list_is_empty(p->steps) )
			free(rove_list_pop(p->steps, HEAD)); /* rove_list_pop() returns the rove_pattern_step_t, free that */
			
		rove_pattern_free(m->data);
		rove_list_remove(state->patterns, m);

		monome_led_off(monome->dev, x, y);

		goto clear_pattern;
	}

	switch( p->status ) {
	case PATTERN_STATUS_RECORDING:
		if( state->pattern_rec != m ) {
			printf("what: pattern is marked as recording but is not the globally-designated recordee\n"
				   "      this shouldn't happen.  visinin fucked up, sorry :(\n");
			goto clear_pattern;
		}
			
		if( !pattern_post_record(state, monome, x, y, m) )
			goto clear_pattern;
			
		/* fall through */

	case PATTERN_STATUS_ACTIVATE:
	case PATTERN_STATUS_INACTIVE:
		p->status = PATTERN_STATUS_ACTIVATE;
		monome_led_on(monome->dev, x, y);
		break;
		
	case PATTERN_STATUS_ACTIVE:
		p->status = PATTERN_STATUS_INACTIVE;
		monome_led_off(monome->dev, x, y);
		break;
	}
	
	return;
	
 clear_pattern:
	self->data = NULL;
	return;
}

static void group_off_handler(rove_monome_handler_t *self, rove_state_t *state, rove_monome_t *monome, const uint8_t x, const uint8_t y, const uint8_t event_type) {
	rove_group_t *group = self->data;
	rove_file_t *f;

	if( event_type != MONOME_BUTTON_DOWN )
		return;
	
	if( !(f = group->active_loop) )
		return; /* group is already off (nothing set as the active loop) */
				
	if( !rove_file_is_active(f) )
		return; /* group is already off (active file not playing) */
				
	/* if there is a pattern being recorded, record this group off */
	if( state->pattern_rec )
		rove_pattern_append_step(state->pattern_rec->data, CMD_GROUP_DEACTIVATE, f, 0);
				
	/* stop the active file */
	rove_file_deactivate(f);
}

static void control_row_handler(rove_monome_handler_t *self, rove_state_t *state, rove_monome_t *monome, const uint8_t x, const uint8_t y, const uint8_t event_type) {
	rove_monome_handler_t *callback;
	
	if( x >= monome->cols )
		return;
	
	if( !(callback = &monome->controls[x]) )
		return;
	
	if( callback->cb )
		return callback->cb(callback, state, monome, x, y, event_type);
	
	switch( event_type ) {
	case MONOME_BUTTON_DOWN:
		if( x == monome->rows - 2 )
			monome->mod_keys |= SHIFT;
		else if( x == monome->rows - 1 )
			monome->mod_keys |= META;
		
		break;
		
	case MONOME_BUTTON_UP:
		if( x == monome->rows - 2 )
			monome->mod_keys &= ~SHIFT;
		else if( x == monome->rows - 1 )
			monome->mod_keys &= ~META;

		break;
	}
}

void file_row_handler(rove_monome_handler_t *self, rove_state_t *state, rove_monome_t *monome, const uint8_t x, const uint8_t y, const uint8_t event_type) {
	rove_file_t *f = self->data;
	unsigned int cols;

	rove_monome_position_t pos = {x, y - f->y};
	
	switch( event_type ) {
	case MONOME_BUTTON_DOWN:
		if( y < f->y || y > ( f->y + f->row_span - 1) )
			return;
		
		cols = (f->columns) ? f->columns : monome->cols;
		if( x > cols - 1 )
			return;

		f->new_offset = calculate_play_pos(f->file_length, pos.x, pos.y,
										   (f->play_direction == FILE_PLAY_DIRECTION_REVERSE), f->row_span, cols);
		
		if( state->pattern_rec )
			rove_pattern_append_step(state->pattern_rec->data, CMD_LOOP_SEEK, f, f->new_offset);
		
		rove_file_on_quantize(f, rove_file_seek);

		break;

	case MONOME_BUTTON_UP:
		break;
	}
}

static void button_handler(const monome_event_t *e, void *user_data) {
	rove_state_t *state = user_data;
	rove_monome_t *monome = state->monome;
	rove_monome_handler_t *callback;

	int event_x, event_y, event_type;
	
	event_x    = e->x;
	event_y    = e->y;
	event_type = e->event_type;

	if( event_y >= monome->rows )
		return;

	if( !(callback = &monome->callbacks[event_y]) )
		return;
	
	if( !callback->cb )
		return;
	
	callback->cb(callback, state, monome, event_x, event_y, event_type);
}

static void initialize_callbacks(rove_state_t *state, rove_monome_t *monome) {
	rove_monome_handler_t *ctrl, *row;
	uint8_t y, row_span;
	int i, group_count;
	
	rove_list_member_t *m;
	rove_file_t *f;
	
	/* leave room for two pattern recorders and the two mod keys */
	group_count = MIN(state->group_count, monome->cols - 4);
	
	y = 0;
	
	for( i = 0; i < group_count; i++ ) {
		ctrl = &monome->controls[i];

		ctrl->pos.x = i;
		ctrl->pos.y = y;

		ctrl->cb    = group_off_handler;
		ctrl->data  = (void *) &state->groups[i];
	}
	
	for( i = monome->cols - 4; i < monome->cols - 2; i++ ) {
		ctrl = &monome->controls[i];

		ctrl->pos.x = i;
		ctrl->pos.y = y;

		ctrl->cb    = pattern_handler;
		ctrl->data  = NULL;
	}
	
	monome->callbacks[y].cb = control_row_handler;

	rove_list_foreach(state->files, m, f) {
		f->mapped_monome = monome;
		row_span = f->row_span;
		y = f->y;
		
		for( i = y; i < y + row_span; i++ ) {
			if( i >= monome->rows )
				continue;

			row = &monome->callbacks[i];
			
			row->pos.x = 0;
			row->pos.y = i;
			
			row->cb    = file_row_handler;
			row->data  = f;
		}
	}
}

void rove_monome_display_file(rove_file_t *f) {
	rove_monome_t *monome = f->mapped_monome;
	rove_monome_position_t pos;
	unsigned int row[2] = {0, 0};
	uint16_t r;

	calculate_monome_pos(f->file_length * f->channels, rove_file_get_play_pos(f), f->row_span, (f->columns) ? f->columns : monome->cols, &pos);

	if( MONOME_POS_CMP(&pos, &f->monome_pos_old) || f->force_monome_update ) {
		if( f->force_monome_update ) {
			if( !f->group->active_loop )
				monome_led_off(f->mapped_monome->dev, f->group->idx, 0);
			else
				monome_led_on(f->mapped_monome->dev, f->group->idx, 0);

			monome->dirty_field &= ~(1 << f->y);
			f->force_monome_update = 0;
		}

		if( pos.y != f->monome_pos_old.y ) 
			monome_led_row_16(monome->dev, f->y + f->monome_pos_old.y, row);

		MONOME_POS_CPY(&f->monome_pos_old, &pos);

		if( rove_file_is_active(f) ) {
			r      = 1 << pos.x;
			row[0] = r & 0x00FF;
			row[1] = r >> 8;
		}

		monome_led_row_16(monome->dev, f->y + pos.y, row);
	}

	MONOME_POS_CPY(&f->monome_pos, &pos);

}

void *rove_monome_loop_thread(void *user_data) {
	monome_t *monome = user_data;
	monome_main_loop(monome);

	return NULL;
}

void rove_monome_run_thread(rove_monome_t *monome) {
	pthread_create(&monome->thread, NULL, rove_monome_loop_thread, monome->dev);
}

void rove_monome_stop_thread(rove_monome_t *monome) {
	pthread_cancel(monome->thread);
}

void rove_monome_free(rove_monome_t *monome) {
	monome_clear(monome->dev, MONOME_CLEAR_OFF);
	monome_close(monome->dev);

	free(monome->callbacks);
	free(monome->controls);

	free(monome);
}

int rove_monome_init(rove_state_t *state, const char *osc_prefix, const char *osc_host_port, const char *osc_listen_port, const int cols, const int rows) {
	rove_monome_t *monome;
	char *buf;
	
	assert(state);
	assert(osc_prefix);
	assert(osc_host_port);
	assert(osc_listen_port);
	assert(cols > 0);
	assert(rows > 0);

	monome = calloc(sizeof(rove_monome_t), 1);

	asprintf(&buf, "osc.udp://127.0.0.1:%s/%s", osc_host_port, osc_prefix);

	if( !(monome->dev = monome_open(buf, "osc", osc_listen_port)) ) {
		free(monome);
		free(buf);
		return -1;
	}

	free(buf);

	monome_register_handler(monome->dev, MONOME_BUTTON_DOWN, button_handler, state);
	monome_register_handler(monome->dev, MONOME_BUTTON_UP, button_handler, state);

	monome->cols     = cols;
	monome->rows     = rows;
	monome->mod_keys = 0;

	monome->quantize_field = 0;
	monome->dirty_field    = 0;
	
	monome->callbacks = calloc(sizeof(rove_monome_handler_t), rows);
	monome->controls  = calloc(sizeof(rove_monome_handler_t), cols);
	initialize_callbacks(state, monome);

	monome_clear(monome->dev, MONOME_CLEAR_OFF);

	state->monome = monome;
	return 0;
}
