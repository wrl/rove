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

#include <stdlib.h>

#include "rove_file.h"
#include "rove_list.h"
#include "rove_pattern.h"

extern rove_state_t state;

rove_pattern_t *rove_pattern_new() {
	rove_pattern_t *self = calloc(sizeof(rove_pattern_t), 1);
	
	self->status = PATTERN_STATUS_INACTIVE;
	self->steps  = rove_list_new();
	self->current_step = NULL;
	
	return self;
}

void rove_pattern_free(rove_pattern_t *self) {
	rove_pattern_step_t *s;
	rove_list_member_t *m;
	
	if( !self )
		return;
	
	rove_list_foreach(self->steps, m, s) {
		rove_list_remove(self->steps, m);
		free(s);
	}
	
	rove_list_free(self->steps);
	free(self);
}

void rove_pattern_append_step(rove_pattern_cmd_t cmd, rove_file_t *f, jack_nframes_t arg) {
	rove_pattern_step_t *s;
	rove_pattern_t *self;

	if( !state.pattern_rec )
		return;

	self = state.pattern_rec->data;
	s = self->steps->tail->prev->data;
	
	if( s && s->file == f && !s->delay ) {
		/* if a step has already been recorded for this file
		   during this quantize tick, we'll overwrite it
		   (effectively leaving us with the final step) */

		s->arg = arg;
		s->cmd = cmd;

		return;
	}
	
	s        = calloc(sizeof(rove_pattern_step_t), 1);
	s->delay = 0;
	s->file  = f;
	s->arg   = arg;
	s->cmd   = cmd;
	
	rove_list_push(self->steps, TAIL, s);
}

#if 0
int rove_pattern_change_status(rove_pattern_t *self, rove_pattern_status_t nstatus) {
	switch( self->status ) {
	case PATTERN_STATUS_INACTIVE:
		switch( nstatus ) {
		case PATTERN_STATUS_RECORDING:
			if( !rove_list_is_empty(self->steps) )
				return 1;
			break;

		default:
			break;
		}

		/* FIXME: currently falls through, eventually would like to
		          eliminate PATTERN_STATUS_ACTIVATE */

	case PATTERN_STATUS_ACTIVATE: 
		switch( nstatus ) {
		case PATTERN_STATUS_ACTIVE:
			self->current_step = self->steps->tail->prev; /* this is counterintuitive */
			self->delay_steps  = ((rove_pattern_step_t *) self->current_step->data)->delay;
			break;

		case PATTERN_STATUS_RECORDING:
			if( !rove_list_is_empty(self->steps) )
				return 1;
			break;

		default:
			break;
		}
		break;

	case PATTERN_STATUS_RECORDING:
		/* deallocate pattern if it contains no steps */
		if( rove_list_is_empty(self->steps) ) {
			self->status = PATTERN_STATUS_INACTIVE;

			if( self->bound_button )
				self->bound_button->data = NULL;

		

	case PATTERN_STATUS_ACTIVE:
	}

	self->status = nstatus;
	return 0;
}
#endif

void rove_pattern_process_patterns() {
	rove_pattern_step_t *s;
	rove_list_member_t *m;
	rove_pattern_t *p;

	rove_list_foreach(state.patterns, m, p) {
		switch( p->status ) {
		case PATTERN_STATUS_ACTIVATE:
			p->status = PATTERN_STATUS_ACTIVE;
			p->current_step = p->steps->tail->prev;
			p->delay_steps = ((rove_pattern_step_t *) p->current_step->data)->delay;
					
		case PATTERN_STATUS_ACTIVE:
			s = p->current_step->data;
					
			while( p->delay_steps >= s->delay ) {
				p->current_step = p->current_step->next;
						
				if( !p->current_step->next )
					p->current_step = p->steps->head->next;
						
				p->delay_steps = 0;
				s = p->current_step->data;
						
				switch( s->cmd ) {
				case CMD_GROUP_DEACTIVATE:
					rove_file_deactivate(s->file);
					break;
							
				case CMD_LOOP_SEEK:
					s->file->new_offset = s->arg;
					rove_file_seek(s->file);
							
					break;
				}
			}
					
			p->delay_steps++;
					
			break;
					
		case PATTERN_STATUS_RECORDING:
			if( !rove_list_is_empty(p->steps) ) {
				((rove_pattern_step_t *) p->steps->tail->prev->data)->delay++;
						
				if( p->delay_steps ) {
					if( --p->delay_steps <= 0 ) {
						p->status = PATTERN_STATUS_ACTIVATE;
						state.pattern_rec = NULL;
					}
				}
			}
					
			break;
					
		default:
			break;
		}
	}
}
