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

#include <stdlib.h>

#include "rove_list.h"
#include "rove_pattern.h"

rove_pattern_t *rove_pattern_new() {
	rove_pattern_t *p = calloc(sizeof(rove_pattern_t), 1);
	
	p->status = PATTERN_STATUS_INACTIVE;
	p->steps  = rove_list_new();
	p->current_step = NULL;
	
	return p;
}

void rove_pattern_free(rove_pattern_t *p) {
	rove_pattern_step_t *s;
	rove_list_member_t *m;
	
	if( !p )
		return;
	
	rove_list_foreach(p->steps, m, s) {
		rove_list_remove(p->steps, m);
		free(s);
	}
	
	rove_list_free(p->steps);
	free(p);
}

void rove_pattern_append_step(rove_pattern_t *p, rove_pattern_cmd_t cmd, rove_file_t *f, jack_nframes_t arg) {
	rove_pattern_step_t *s = p->steps->tail->prev->data;
	
	if( s ) {
		if( s->file == f && !s->delay )
			return;
	}
	
	s        = calloc(sizeof(rove_pattern_step_t), 1);
	s->delay = 0;
	s->file  = f;
	s->arg   = arg;
	s->cmd   = cmd;
	
	rove_list_push(p->steps, TAIL, s);
}
