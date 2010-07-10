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

extern rove_state_t state;

static void pattern_handler(void *handler, rove_monome_t *monome, const int x, const int y, const int event_type) {
	return;
}

static void group_off_handler(void *handler, rove_monome_t *monome, const int x, const int y, const int event_type) {
	rove_group_t *group = HANDLER_T(handler)->data;
	rove_file_t *f;

	if( event_type != MONOME_BUTTON_DOWN ||
		!(f = group->active_loop) ||	/* group is already off (nothing set as the active loop) */
		!rove_file_is_active(f) )		/* group is already off (active file not playing) */
		return;

	rove_file_deactivate(f);
}

static void control_row_handler(void *handler, rove_monome_t *monome, const int x, const int y, const int event_type) {
	rove_monome_handler_t *callback;
	
	if( x >= monome->cols || !(callback = &monome->controls[x]) )
		return;
	
	if( callback->cb )
		return callback->cb(callback, monome, x, y, event_type);
	
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

void file_row_handler(void *handler, rove_monome_t *monome, const int x, const int y, const int event_type) {
	rove_file_t *f = HANDLER_T(handler)->data;

	if( f->monome_in_cb )
		f->monome_in_cb(f, monome, x, y, event_type); 
}

static void button_handler(const monome_event_t *e, void *user_data) {
	rove_monome_t *monome = user_data;
	rove_monome_handler_t *callback;

	int event_x, event_y, event_type;
	
	event_x    = e->x;
	event_y    = e->y;
	event_type = e->event_type;

	if( event_y >= monome->rows ||
		!(callback = &monome->callbacks[event_y]) ||
		!callback->cb )
		return;
	
	callback->cb(callback, monome, event_x, event_y, event_type);
}

static void initialize_callbacks(rove_monome_t *monome) {
	rove_monome_handler_t *ctrl, *row;
	int y, row_span, i, group_count;
	
	rove_list_member_t *m;
	rove_file_t *f;
	
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

	rove_list_foreach(state.files, m, f) {
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

int rove_monome_init() {
	rove_monome_t *monome;
	char *buf;
	
	assert(state.config.osc_prefix);
	assert(state.config.osc_host_port);
	assert(state.config.osc_listen_port);
	assert(state.config.cols > 0);
	assert(state.config.rows > 0);

	monome = calloc(sizeof(rove_monome_t), 1);

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
	
	monome->callbacks = calloc(sizeof(rove_monome_handler_t), state.config.rows);
	monome->controls  = calloc(sizeof(rove_monome_handler_t), state.config.cols);
	initialize_callbacks(monome);

	monome_clear(monome->dev, MONOME_CLEAR_OFF);

	state.monome = monome;
	return 0;
}
