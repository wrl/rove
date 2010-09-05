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
#include <stdio.h>

#include "types.h"
#include "file.h"
#include "list.h"
#include "pattern.h"

extern state_t state;

void pattern_record(file_t *victim, uint_t x, uint_t y, uint_t type) {
	pattern_t *p = state.pattern_rec;
	pattern_step_t *step;

	if( !p )
		return;

	if( !(step = calloc(1, sizeof(pattern_step_t))) ) {
		fprintf(stderr, "WARNING: can't allocate pattern step.\n");
		return;
	}

	step->delay = 0;
	step->victim = victim;
	step->x = x;
	step->y = y;
	step->type = type;

	list_push_raw(&p->steps, TAIL, LIST_MEMBER_T(step));
	p->current_step = step;
}

void pattern_status_set(pattern_t *self, pattern_status_t nstatus) {
	switch( self->status ) {
	case PATTERN_STATUS_INACTIVE:
		break;

	case PATTERN_STATUS_ACTIVE:
		break;

	case PATTERN_STATUS_RECORDING:
		if( state.pattern_rec != self )
			fprintf(stderr, "state.pattern_rec is fucked, aieee!\n");
		else
			state.pattern_rec = NULL;

		self->current_step = PATTERN_STEP_T(self->steps.tail.prev);
		self->step_delay = 0;
	}

	self->status = nstatus;
}

void pattern_process(pattern_t *self) {
	pattern_step_t *step = self->current_step;

	switch( self->status ) {
	case PATTERN_STATUS_INACTIVE:
		break;

	case PATTERN_STATUS_RECORDING:
		if( stlist_is_empty(self->steps) )
			break;

		step->delay++;

		if( self->step_delay && --self->step_delay <= 0 )
			pattern_status_set(self, PATTERN_STATUS_ACTIVE);
			/* also, fall through */
		else
			return;

	case PATTERN_STATUS_ACTIVE:
		if( self->step_delay ) {
			self->step_delay--;
			break;
		}

		do {
			step->victim->monome_in_cb(
				self->monome, step->x, step->y, step->type, step->victim);

			if( LIST_MEMBER_T(step)->next == &self->steps.tail )
				step = PATTERN_STEP_T(self->steps.head.next);
			else
				step = PATTERN_STEP_T(LIST_MEMBER_T(step)->next);
		} while( !step->delay );

		self->current_step = step;
		self->step_delay = step->delay;

		break;
	}
}

void pattern_free(pattern_t *self) {
	pattern_step_t *step;

	assert(self);

	while( (step = PATTERN_STEP_T(list_pop_raw(&self->steps, HEAD))) )
		free(step);

	free(self); /* so liberating */
}

pattern_t *pattern_new() {
	pattern_t *self = calloc(1, sizeof(pattern_t));
	list_init(&self->steps);

	return self;
}
