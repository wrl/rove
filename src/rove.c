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

#include <getopt.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "rove.h"
#include "rove_file.h"
#include "rove_jack.h"
#include "rove_list.h"
#include "rove_monome.h"

#define DEFAULT_OSC_PREFIX      "rove"
#define DEFAULT_OSC_HOST_PORT   "8080"
#define DEFAULT_OSC_LISTEN_PORT "8000"

#define MAX_LENGTH 1024

/* 48 is ASCII '0', 57 is ASCII '9'. tests one character. */
#define is_numeric(c) ((48 <= c) && (c <= 57))

static rove_group_t *initialize_groups(const int group_count) {
	rove_group_t *groups;
	int i;
	
	/**
	 * leave room for the four control buttons on the top row
	 */

	if( !(groups = calloc(sizeof(rove_group_t), group_count)) )
		return NULL;

	for( i = 0; i < group_count; i++ )
		groups[i].idx = i;
	
	return groups;
}

/**
 * this is a mess and i should probably either rewrite it or use an external library
 */
static int parse_conf_file(const char *path, rove_state_t *state) {
	char line[MAX_LENGTH], sndpath[MAX_LENGTH], *offset;
	uint8_t y = 1, span, group, rev;
	rove_file_t *cur;
	uint16_t len;
	FILE *conf;
	
	if( !(conf = fopen(path, "r")) )
		return 1;
	
	chdir(dirname((char *) path));
	
	while( fgets(line, MAX_LENGTH, conf) ) {
		len = strlen(line);
		
		if( line[0] == ':' ) {
			if( state->bpm || !(offset = strstr(line + 1, ":")) )
				return 1;
			
			state->bpm = strtod(line + 1, &offset);
			state->group_count = atoi(++offset);
			
			if( !(offset = strstr(offset, ":")) )
				return 1;
			state->pattern_lengths[0] = atoi(++offset);
			
			if( !(offset = strstr(offset, ":")) )
				return 1;
			state->pattern_lengths[1] = atoi(++offset);
			
			if( !(offset = strstr(offset, ":")) )
				state->beat_multiplier = 0;
			else
				state->beat_multiplier = strtod(++offset, NULL);

			if( !(state->groups = initialize_groups(state->group_count)) )
				return 1;
		} else {
			if( !state->bpm || !state->group_count )
				return 1;
			
			if( !(group = atoi(line)) )
				return 1;
			
			if( group > state->group_count )
				return 1;
			
			if( !(offset = strstr(line, ":")) )
				return 1;
			
			offset += 1;
			len    -= offset - line;
			
			if( *offset == 'r' ) {
				rev = 1;
				offset++;
			} else
				rev = 0;
			
			if( !(span = atoi(offset)) )
				return 1;
			
			if( !(offset = strstr(offset, ":")) )
				return 1;
			
			offset += 1;
			len    -= offset - line - 2;
			
			strncpy(sndpath, offset, MAX_LENGTH - 1);
			sndpath[len] = 0;
			
			if( sndpath[len - 1] == '\n' )
				sndpath[len - 1] = 0;
			
			if( y + span > 16 ) {
				printf("\n\t(you tried to load more, but you're out of room!)\n");
				break;
			}
			
			/**
			 * now that we've parsed out the relevant information, allocate and initialize a rove_file_t.
			 */
			
			if( !(cur = rove_file_new_from_path(sndpath)) )
				continue;
			
			cur->y = y;
			cur->row_span = span;
			cur->group = &state->groups[group - 1];
			cur->play_direction = ( rev ) ? FILE_PLAY_DIRECTION_REVERSE : FILE_PLAY_DIRECTION_FORWARD;
			
			rove_list_push(state->files, TAIL, cur);
			
			printf("\t%d - %d\t%s\n", y, y + span - 1, sndpath);
				   
			y += span;
			
			cur  = NULL;
		}
	}
	
	fclose(conf);
	
	return 0;
}

static void usage() {
	printf("Usage: rove [OPTION]... session_file.rv\n"
		   "  -c, --monome-columns=COLUMNS\n"
		   "  -p, --osc-prefix=PREFIX\n"
		   "  -h, --osc-host-port=PORT\n"
		   "  -l, --osc-listen-port=PORT\n");
}

static void main_loop(rove_state_t *state) {
	rove_list_member_t *m;
	rove_file_t *f;

	while(1) {
		pthread_cond_wait(&state->monome_display_notification, &state->monome_mutex);
		
		rove_list_foreach(state->files, m, f) {
			switch( f->state ) {
			case FILE_STATE_DEACTIVATE:
				rove_file_deactivate(f);
				rove_monome_blank_file_row(state, f);
				
				if( f->group->active_loop == f )
					monome_led_off(state->monome, f->group->idx, 0);
				
			case FILE_STATE_INACTIVE:
				continue;
				
			case FILE_STATE_ACTIVATE:
			case FILE_STATE_ACTIVE:
			case FILE_STATE_RESEEK:
				rove_monome_display_file(state, f);
				break;
			}
		}
	}
}

void rove_recalculate_bpm_variables(rove_state_t *state) {
	state->snap_delay = lrint(((60 / state->bpm) * state->beat_multiplier) * ((double) jack_get_sample_rate(state->client)));
}

int main(int argc, char **argv) {
	char *osc_prefix, *osc_host_port, *osc_listen_port, *session_file, c;
	rove_state_t state;
	uint8_t cols;
	int i;
	
	struct option arguments[] = {
		{"monome-columns",	required_argument, 0, 'c'},
		{"osc-prefix", 		required_argument, 0, 'p'},
		{"osc-host-port", 	required_argument, 0, 'h'},
		{"osc-listen-port",	required_argument, 0, 'l'},
		{0, 0, 0, 0}
	};
	
	memset(&state, 0, sizeof(rove_state_t));
	
	cols            = 8;
	session_file    = NULL;
	osc_prefix      = DEFAULT_OSC_PREFIX;
	osc_host_port   = DEFAULT_OSC_HOST_PORT;
	osc_listen_port = DEFAULT_OSC_LISTEN_PORT;
	
	opterr = 0;
	
	while( (c = getopt_long(argc, argv, "c:p:h:l:", arguments, &i)) > 0 ) {
		switch( c ) {
		case 'c':
			cols = atoi(optarg) & 0xF;
			
			if( !cols )
				cols = 8;
			
			break;
			
		case 'p':
			osc_prefix = optarg;
			
			if( *osc_prefix == '/' ) /* remove the leading slash if there is one */
				osc_prefix++;
			
			break;
			
		case 'h':
			osc_host_port = optarg;
			
			do {
				if( !is_numeric(*optarg) ) {
					usage();
					printf("\nerror: \"%s\" is not a valid host port.\n\n", osc_host_port);
					return 1;
				}
			} while( *++optarg );

			break;
			
		case 'l':
			osc_listen_port = optarg;

			do {
				if( !is_numeric(*optarg) ) {
					usage();
					printf("\nerror: \"%s\" is not a valid listen port.\n\n", osc_listen_port);
					return 1;
				}
			} while( *++optarg );


			break;
		}
	}
	
	if( optind == argc ) {
		usage();
		printf("\nerror: you did not specify a session file!\n\n");
		return 1;
	}
	
	session_file = argv[optind];
	
	printf("\nhey, welcome to rove!\n\n");
	
	if( rove_jack_init(&state) )
		return 1;
	
	state.files    = rove_list_new();
	state.patterns = rove_list_new();

	state.active   = rove_list_new();
	state.staging  = rove_list_new();
	
	pthread_mutex_init(&state.monome_mutex, NULL);
	pthread_cond_init(&state.monome_display_notification, NULL);

	printf("you've got the following loops loaded:\n"
		   "\t[rows]\t[file]\n");
	
	if( parse_conf_file(session_file, &state) ) {
		printf("error parsing session file :(\n");
		return 1;
	}
	
	if( rove_list_is_empty(state.files) ) {
		fprintf(stderr, "\t(none, evidently.  get some and come play!)\n\n");
		return 1;
	}
	
	rove_recalculate_bpm_variables(&state);
	state.frames = state.snap_delay;
	
	if( rove_monome_init(&state, osc_prefix, osc_host_port, osc_listen_port, cols) )
		return 1;

	if( rove_jack_activate(&state) )
		return 1;
	
	rove_monome_run_thread(state.monome);
	main_loop(&state);

	return 0;
}
