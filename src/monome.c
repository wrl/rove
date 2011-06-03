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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <monome.h>
#include <sndfile.h>

#include "rove.h"
#include "file.h"
#include "jack.h"
#include "list.h"
#include "util.h"
#include "rmonome.h"
#include "session.h"
#include "pattern.h"

#define SHIFT 0x01
#define META  0x02

extern state_t state;

static void initialize_file_callbacks(r_monome_t *monome);

static int remove_pattern(pattern_t *p, r_monome_t *monome, uint_t x, uint_t y) {
	pattern_status_set(p, PATTERN_STATUS_INACTIVE);
	list_remove_raw(LIST_MEMBER_T(p));
	pattern_free(p);

	monome_led_off(monome->dev, x, y);
	return 1;
}

static int finalize_pattern(pattern_t *p, r_monome_t *monome, uint_t x, uint_t y) {
	if( !stlist_is_empty(p->steps) ) {
		pattern_status_set(p, PATTERN_STATUS_ACTIVE);
		monome_led_on(monome->dev, x, y);
		return 0;
	}

	return remove_pattern(p, monome, x, y);
}

static void pattern_handler(r_monome_t *monome, uint_t x, uint_t y, uint_t event_type, void *user_arg) {
	pattern_t **pptr = ((pattern_t **) &HANDLER_T(user_arg)->data),
			  *pattern = *pptr;
	int pat_idx = 4 - monome->cols + x;

	if( event_type != MONOME_BUTTON_DOWN )
		return;

	if( !pattern ) {
		if( state.pattern_rec )
			finalize_pattern(state.pattern_rec, monome, x, y);

		pattern = *pptr = pattern_new();
		pattern->idx    = pat_idx;
		pattern->monome = monome;
		pattern->status = PATTERN_STATUS_RECORDING;

		if( state.pattern_lengths[pat_idx] )
			pattern->step_delay = rintf(floor(
					state.pattern_lengths[pat_idx] / state.beat_multiplier));
		else
			pattern->step_delay = 0;

		list_push_raw(state.patterns, TAIL, LIST_MEMBER_T(pattern));
		state.pattern_rec = pattern;

		monome_led_on(monome->dev, x, y);
		return;
	}

	if( pattern->status == PATTERN_STATUS_RECORDING ) {
		if( finalize_pattern(pattern, monome, x, y) )
			*pptr = NULL;
	} else {
		remove_pattern(pattern, monome, x, y);
		*pptr = NULL;
	}
}

static void group_off_handler(r_monome_t *monome, uint_t x, uint_t y, uint_t event_type, void *user_arg) {
	group_t *group = HANDLER_T(user_arg)->data;
	file_t *f;

	pattern_record(group_off_handler, user_arg, x, y, event_type);

	if( event_type != MONOME_BUTTON_DOWN
		|| !(f = group->active_loop)  /* group is already off (nothing set as the active loop) */
		|| !file_is_active(f) )       /* group is already off (active file not playing) */
		return;

	file_deactivate(f);

	if( f->monome_out_cb )
		f->monome_out_cb(f, state.monome);
}

static void session_lights(r_monome_t *monome) {
	monome_led_set(monome->dev, monome->cols - 1, 0,
	               !!LIST_MEMBER_T(state.active_session)->next->next);
	monome_led_set(monome->dev, monome->cols - 2, 0,
	               !!LIST_MEMBER_T(state.active_session)->prev->prev);
}

static void control_row_handler(r_monome_t *monome, uint_t x, uint_t y, uint_t event_type, void *user_arg) {
	r_monome_handler_t *callback;
	int i;

	if( x >= monome->cols || !(callback = &monome->controls[x]) )
		return;

	if( callback->cb )
		return callback->cb(monome, x, y, event_type, callback);

	if( event_type != MONOME_BUTTON_DOWN )
		return;

	switch( monome->cols - x ) {
	case 2:
		if( session_prev() )
			return;
		break;

	case 1:
		if( session_next() )
			return;
		break;
		
	default:
		return;
	}

	initialize_file_callbacks(monome);

	for( i = 0; i < state.group_count; i++ )
		if( state.groups[i].active_loop )
			file_force_monome_update(state.groups[i].active_loop);

	session_lights(monome);
}

void file_row_handler(r_monome_t *monome, uint_t x, uint_t y, uint_t event_type, void *user_arg) {
	file_t *f = HANDLER_T(user_arg)->data;

	if( !f->monome_in_cb )
		return;

	pattern_record(f->monome_in_cb, f, x, y, event_type);
	f->monome_in_cb(monome, x, y, event_type, f); 
}

static void button_handler(const monome_event_t *e, void *user_data) {
	r_monome_t *monome = user_data;
	r_monome_handler_t *callback;

	int event_x, event_y, event_type;

	event_x    = e->grid.x;
	event_y    = e->grid.y;
	event_type = e->event_type;

	if( event_y >= monome->rows ||
		!(callback = &monome->callbacks[event_y]) ||
		!callback->cb )
		return;

	callback->cb(monome, event_x, event_y, event_type, callback);
}

static void initialize_file_callbacks(r_monome_t *monome) {
	r_monome_handler_t *row;

	int i, y, row_span;
	list_member_t *m;
	file_t *f;

	for( i = 1; i < monome->rows; i++ ) {
		monome->callbacks[i].cb = NULL;
		monome->callbacks[i].data = NULL;
	}

	list_foreach(state.files, m, f) {
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

	for( y += row_span; y < monome->rows; y++ )
		monome->callbacks[y].cb = NULL;
}

static void initialize_control_callbacks(r_monome_t *monome) {
	r_monome_handler_t *ctrl;
	int y, i, group_count;

	/* leave room for two pattern recorders and the two mod keys */
	group_count = MIN(state.group_count, monome->cols - 4);

	y = 0;

	for( i = 0; i < group_count; i++ ) {
		ctrl = &monome->controls[i];

		ctrl->pos.x = i;
		ctrl->pos.y = y;

		ctrl->cb    = group_off_handler;
		ctrl->data  = (void *) &state.groups[i];
	}

	for( i = monome->cols - 4; i < monome->cols - 2; i++ ) {
		ctrl = &monome->controls[i];

		ctrl->pos.x = i;
		ctrl->pos.y = y;

		ctrl->cb    = pattern_handler;
		ctrl->data  = NULL;
	}

	monome->callbacks[y].cb = control_row_handler;
}

static void initialize_callbacks(r_monome_t *monome) {
	initialize_control_callbacks(monome);
	initialize_file_callbacks(monome);
}

void *r_monome_loop_thread(void *user_data) {
	monome_t *monome = user_data;
	monome_event_loop(monome);

	return NULL;
}

void r_monome_run_thread(r_monome_t *monome) {
	pthread_create(&monome->thread, NULL, r_monome_loop_thread, monome->dev);
}

void r_monome_stop_thread(r_monome_t *monome) {
	pthread_cancel(monome->thread);
}

void r_monome_free(r_monome_t *monome) {
	monome_led_all(monome->dev, 0);
	monome_close(monome->dev);

	free(monome->callbacks);
	free(monome->controls);

	free(monome);
}

int r_monome_init() {
	r_monome_t *monome;
	char *buf;

	assert(state.config.osc_prefix);
	assert(state.config.osc_host_port);
	assert(state.config.osc_listen_port);
	assert(state.config.cols > 0);
	assert(state.config.rows > 0);

	monome = calloc(sizeof(r_monome_t), 1);

	asprintf(&buf, "osc.udp://127.0.0.1:%s/%s", state.config.osc_host_port, state.config.osc_prefix);

	if( !(monome->dev = monome_open(buf, state.config.osc_listen_port)) ) {
		free(monome);
		free(buf);
		return -1;
	}

	free(buf);

	monome_register_handler(monome->dev, MONOME_BUTTON_DOWN, button_handler, monome);
	monome_register_handler(monome->dev, MONOME_BUTTON_UP, button_handler, monome);

	/* eventually we will support several monomes */
	monome->cols     = state.config.cols;
	monome->rows     = state.config.rows;
	monome->mod_keys = 0;

	monome->quantize_field = 0;
	monome->dirty_field    = 0;

	monome->callbacks = calloc(sizeof(r_monome_handler_t), state.config.rows);
	monome->controls  = calloc(sizeof(r_monome_handler_t), state.config.cols);
	initialize_callbacks(monome);

	monome_led_all(monome->dev, 0);

	state.monome = monome;

	session_lights(monome);
	return 0;
}
