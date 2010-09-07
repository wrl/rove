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

#include "config_parser.h"
#include "session.h"
#include "rove.h"


#define SESSION_T(x) ((session_t *) x)


extern state_t state;

static group_t *initialize_groups(uint_t group_count) {
	group_t *groups;
	int i;

	if( !(groups = calloc(sizeof(group_t), group_count)) )
		return NULL;

	for( i = 0; i < group_count; i++ )
		groups[i].idx = i;

	return groups;
}

static void file_section_callback(const conf_section_t *section, void *arg) {
	session_t *session = *((session_t **) arg);
	static int y = 1;

	unsigned int e, c, r, group, reverse, *v;
	file_t *f;
	double speed;
	char *path;

	conf_pair_t *pair = NULL;

	if( !session ) {
		fprintf(stderr, "file block specified before session block, aieee!\n");
		assert(session);
	}

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

	if( !(f = file_new_from_path(path)) )
		goto out;

	if( group > state.group_count )
		group = state.group_count;

	f->y = y;
	f->speed = speed;
	f->row_span = r;
	f->columns  = (c) ? ((c - 1) & 0xF) + 1 : session->cols;
	f->group = &state.groups[group - 1];
	f->play_direction = ( reverse ) ? FILE_PLAY_DIRECTION_REVERSE : FILE_PLAY_DIRECTION_FORWARD;

	list_push(state.files, TAIL, f);
	printf("\t%d - %d\t%s\n", y, y + r - 1, path);

	y += r;

out:
	free(path);
	return;
}

static void session_section_callback(const conf_section_t *section, void *arg) {
	session_t *session, **sptr = arg;
	conf_pair_t *pair = NULL;
	char v;

	if( !*sptr ) {
		if( !(*sptr = session_new()) ) {
			fprintf(stderr, "couldn't allocate sptr_t, aieee!\n");
			assert(*sptr);
		}
	}

	session = *sptr;
	session->cols = 0;

	while( (v = conf_getvar(section, &pair)) ) {
		switch( v ) {
		case 'q': /* quantize */
			session->beat_multiplier = strtod(pair->value, NULL);
			break;

		case 'b': /* bpm */
			session->bpm = strtod(pair->value, NULL);
			break;

		case 'g': /* groups */
			if( !state.group_count )
				state.group_count = (int) strtol(pair->value, NULL, 10);

			break;

		case '1': /* pattern1 */
			session->pattern_lengths[0] = (int) strtol(pair->value, NULL, 10);
			break;

		case '2': /* pattern2 */
			session->pattern_lengths[1] = (int) strtol(pair->value, NULL, 10);
			break;

		case 'c': /* columns */
			session->cols = (uint_t) strtoul(pair->value, NULL, 10);
		}
	}

	if( !state.groups )
		state.groups = initialize_groups(state.group_count);
}

void session_activate(session_t *self) {
	state.beat_multiplier = self->beat_multiplier;
	state.bpm = self->bpm;

	state.pattern_lengths = self->pattern_lengths;
}

session_t *session_new() {
	session_t *self = calloc(1, sizeof(session_t));

	list_push_raw(&state.sessions, TAIL, LIST_MEMBER_T(self));
	return self;
}

void session_free(session_t *self) {
	list_remove_raw(LIST_MEMBER_T(self));
	free(self);
}

int session_load(const char *path) {
	session_t *session = NULL;

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
		{"quantize", NULL, DOUBLE, 'q'},
		{"bpm",      NULL, DOUBLE, 'b'},
		{"groups",   NULL,    INT, 'g'},
		{"pattern1", NULL,    INT, '1'},
		{"pattern2", NULL,    INT, '2'},
		{"columns",  NULL,    INT, 'c'},
		{NULL}
	};

	conf_section_t config_sections[] = {
		{"session", session_vars, session_section_callback, &session},
		{"file",    file_vars   , file_section_callback, &session},
		{NULL}
	};

	if( conf_load(path, config_sections, 1) )
		return 1;

	session_activate(session);

	return 0;
}
