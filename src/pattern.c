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

#include "file.h"
#include "list.h"
#include "pattern.h"

extern state_t state;

void pattern_record(file_t *victim, uint_t x, uint_t y, uint_t pressed) {
	if( state.pattern_rec )
		printf("good\n");
}

void pattern_status_set(pattern_t *self, pattern_status_t nstatus) {
	printf("%d\n", nstatus);
	self->status = nstatus;
}

void pattern_process(pattern_t *self) {
	switch( self->status ) {
	case PATTERN_STATUS_INACTIVE:
		return;

	case PATTERN_STATUS_ACTIVE:
		return;

	case PATTERN_STATUS_RECORDING:
		return;
	}
}

void pattern_free(pattern_t *self) {
	pattern_step_t *step;

	assert(self);

	while( (step = PATTERN_STEP_T(list_pop_raw(&self->steps, HEAD))) )
		free(step);

	free(self); /* so liberating */
	puts("freed");
}

pattern_t *pattern_new() {
	pattern_t *self = calloc(1, sizeof(pattern_t));
	list_init(&self->steps);

	puts("allocd");
	return self;
}
