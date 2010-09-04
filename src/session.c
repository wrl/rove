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

#include "config_parser.h"
#include "rove.h"


extern rove_state_t state;
static int session_cols;

static rove_group_t *initialize_groups(uint_t group_count) {
	rove_group_t *groups;
	int i;
	
	if( !(groups = calloc(sizeof(rove_group_t), group_count)) )
		return NULL;

	for( i = 0; i < group_count; i++ )
		groups[i].idx = i;
	
	return groups;
}

static void file_section_callback(const conf_section_t *section, void *arg) {
	static int y = 1;

	unsigned int e, c, r, group, reverse, *v;
	rove_file_t *f;
	double speed;
	char *path;
	
	conf_pair_t *pair = NULL;
	
	path    = NULL;
	group   = 0;
	r       = 1;
	c       = 0;
	reverse = 0;
	speed   = 1.0;
	
	while( (e = conf_getvar(section, &pair)) ) {
		switch( e ) {
		case 'p': /* file path */
			path = pair->value;
			continue;

		case 'v': /* reverse */
			reverse = 1;
			continue;

		case 's': /* speed */
			speed = strtod(pair->value, NULL);
			continue;

		case 'g': /* group */
			v = &group;
			break;
			
		case 'c': /* columns */
			v = &c;
			break;

		case 'r': /* rows */
			v = &r;
			break;
		}
		
		*v = (unsigned int) strtol(pair->value, NULL, 10);
	}
	
	if( !path ) {
		printf("no file path specified in file section starting at line %d\n", section->start_line);
		return;
	}
	
	if( !group ) {
		printf("no group specified in file section starting at line %d\n", section->start_line);
		goto out;
	}
	
	if( !(f = rove_file_new_from_path(path)) )
		goto out;
	
	if( group > state.group_count )
		group = state.group_count;
	
	f->y = y;
	f->speed = speed;
	f->row_span = r;
	f->columns  = (c) ? ((c - 1) & 0xF) + 1 : session_cols;
	f->group = &state.groups[group - 1];
	f->play_direction = ( reverse ) ? FILE_PLAY_DIRECTION_REVERSE : FILE_PLAY_DIRECTION_FORWARD;
	
	rove_list_push(state.files, TAIL, f);
	printf("\t%d - %d\t%s\n", y, y + r - 1, path);
	
	y += r;
	
 out:
	free(path);
	return;
}

static void session_section_callback(const conf_section_t *section, void *arg) {
	conf_default_section_callback(section);

	if( !state.groups )
		state.groups = initialize_groups(state.group_count); /* FIXME: can return null, handle properly */
}

int session_load(const char *path) {
	conf_var_t file_vars[] = {
		{"path",    NULL, STRING, 'p'},
		{"groups",  NULL,    INT, 'g'},
		{"columns", NULL,    INT, 'c'},
		{"rows",    NULL,    INT, 'r'},
		{"reverse", NULL,   BOOL, 'v'},
		{"speed",   NULL, DOUBLE, 's'},
		{NULL}
	};
	
	conf_var_t session_vars[] = {
		{"quantize", &state.beat_multiplier, DOUBLE, 'q'},
		{"bpm",      &state.bpm,             DOUBLE, 'b'},
		{"groups",   &state.group_count,        INT, 'g'},
		{"pattern1", &state.pattern_lengths[0], INT, '1'},
		{"pattern2", &state.pattern_lengths[1], INT, '2'},
		{"columns",  &session_cols,             INT, 'c'},
		{NULL}
	};
	
	conf_section_t config_sections[] = {
		{"session", session_vars, session_section_callback},
		{"file",    file_vars   , file_section_callback},
		{NULL}
	};
	
	session_cols = 0;

	if( conf_load(path, config_sections, 1) )
		return 1;
	
	return 0;
}

