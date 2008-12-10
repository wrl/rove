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
#include <glob.h>

#include "rove.h"
#include "rove_file.h"
#include "rove_jack.h"
#include "rove_list.h"
#include "rove_config.h"
#include "rove_monome.h"

#define DEFAULT_CONF_FILE_NAME  ".rove.conf"

#define DEFAULT_MONOME_COLUMNS  8
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
				if( f->group->active_loop == f )
					monome_led_off(state->monome, f->group->idx, 0);
				
				rove_file_deactivate(f);
				rove_monome_blank_file_row(state->monome, f);
				
			case FILE_STATE_INACTIVE:
				continue;
				
			default:
				rove_monome_display_file(state, f);
				break;
			}
		}
	}
}

static char *user_config_path() {
	char *home, *path, *conf;
	
	if( !(home = getenv("HOME")) )
		return NULL;
	
	if( home[strlen(home) - 1] == '/' )
		conf++;

	asprintf(&path, "%s/%s", home, DEFAULT_CONF_FILE_NAME);
	return path;
}

static void file_section_callback(const rove_config_section_t *section, void *arg) {
	rove_state_t *state = arg;
	static unsigned int y = 1;

	unsigned int group, rows, cols, reverse, *v;
	rove_file_t *f;
	char *path;
	
	rove_config_pair_t *pair = NULL;
	int c;
	
	path    = NULL;
	group   = 0;
	rows    = 1;
	cols    = 0;
	reverse = 0;
	
	while( (c = rove_config_getvar(section, &pair)) ) {
		switch( c ) {
		case 'p':
			path = pair->value;
			continue;
			
		case 'v':
			reverse = 1;
			continue;

		case 'g':
			v = &group;
			break;
			
		case 'c':
			v = &cols;
			break;

		case 'r':
			v = &rows;
			break;
		}
		
		*v = (int) strtol(pair->value, NULL, 10);
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
	
	if( group > state->group_count )
		group = state->group_count;
	
	f->y = y;
	f->row_span = rows;
	f->columns  = (cols) ? ((cols - 1) & 0xF) + 1 : 0;
	f->group = &state->groups[group - 1];
	f->play_direction = ( reverse ) ? FILE_PLAY_DIRECTION_REVERSE : FILE_PLAY_DIRECTION_FORWARD;
	
	rove_list_push(state->files, TAIL, f);
	printf("\t%d - %d\t%s\n", y, y + rows - 1, path);
	
	y += rows;
	
 out:
	free(path);
	return;
}

static void session_section_callback(const rove_config_section_t *section, void *arg) {
	rove_state_t *state = arg;
	
	rove_config_default_section_callback(section);
	state->groups = initialize_groups(state->group_count);

}

static int load_session_file(const char *path, rove_state_t *state, int *c) {
	int cols;

	rove_config_var_t file_vars[] = {
		{"path",    NULL, STRING, 'p'},
		{"groups",  NULL,    INT, 'g'},
		{"columns", NULL,    INT, 'c'},
		{"rows",    NULL,    INT, 'r'},
		{"reverse", NULL,   BOOL, 'v'},
		{NULL}
	};
	
	rove_config_var_t session_vars[] = {
		{"quantize", &state->beat_multiplier, DOUBLE, 'q'},
		{"bpm",      &state->bpm,             DOUBLE, 'b'},
		{"groups",   &state->group_count,        INT, 'g'},
		{"pattern1", &state->pattern_lengths[0], INT, '1'},
		{"pattern2", &state->pattern_lengths[1], INT, '2'},
		{"columns",  &cols,                      INT, 'c'},
		{NULL}
	};
	
	rove_config_section_t config_sections[] = {
		{"session", session_vars, session_section_callback, state},
		{"file",    file_vars   , file_section_callback,    state},
		{NULL}
	};
	
	if( rove_load_config(path, config_sections, 1) )
		return 0;
	
	if( cols && !*c )
		*c = ((cols - 1) & 0xF) + 1;
	
	return 0;
}

static int load_user_conf(int *c, char **op, char **ohp, char **olp) {
	char *osc_prefix, *osc_host_port, *osc_listen_port, *conf;
	int cols;

	rove_config_var_t monome_vars[] = {
		{"columns", &cols, INT, 'c'},
		{NULL}
	};
	
	rove_config_var_t osc_vars[] = {
		{"prefix",      &osc_prefix,      STRING, 'p'},
		{"host-port",   &osc_host_port,   STRING, 'h'},
		{"listen-port", &osc_listen_port, STRING, 'h'},
		{NULL}
	};
	
	rove_config_section_t config_sections[] = {
		{"monome", monome_vars},
		{"osc",    osc_vars},
		{NULL}
	};
	
	cols            = 0;
	osc_prefix      = NULL;
	osc_host_port   = NULL;
	osc_listen_port = NULL;

	if( !(conf = user_config_path()) )
		return 0;
	
	if( rove_load_config(conf, config_sections, 0) )
		return 0;
	
	free(conf);
	
	if( cols && !*c )
		*c = ((cols - 1) & 0xF) + 1;
	
	if( osc_prefix && !*op ) {
		if( *osc_prefix == '/' ) { /* remove the leading slash if there is one */
			conf = strdup(osc_prefix + 1);
			free(osc_prefix);
			osc_prefix = conf;
		}
		
		if( *op )
			free(*op);
		*op = osc_prefix;
	}
	
	if( osc_host_port && !*ohp ) {
		conf = osc_host_port;
		do {
			if( !is_numeric(*conf) ) {
				usage();
				printf("\nrove_config: \"%s\" is not a valid host port."
					   "\n             please check your conf file!\n", osc_host_port);
				return 1;
			}
		} while( *++conf );
		
		if( *ohp )
			free(*ohp);
		*ohp = osc_host_port;
	}
	
	if( osc_listen_port && !*olp ) {
		conf = osc_listen_port;
		do {
			if( !is_numeric(*conf) ) {
				usage();
				printf("\nrove_config: \"%s\" is not a valid listen port."
					   "\n             please check your conf file!\n", osc_listen_port);
				return 1;
			}
		} while( *++conf );
		
		if( *olp )
			free(*olp);
		*olp = osc_listen_port;
	}

	return 0;
}

static void rove_recalculate_bpm_variables(rove_state_t *state) {
	state->snap_delay = lrint(((60 / state->bpm) * state->beat_multiplier) * ((double) jack_get_sample_rate(state->client)));
}

int main(int argc, char **argv) {
	char *osc_prefix, *osc_host_port, *osc_listen_port, *session_file, c;
	rove_state_t state;
	int cols, i;
	
	struct option arguments[] = {
		{"monome-columns",	required_argument, 0, 'c'},
		{"osc-prefix", 		required_argument, 0, 'p'},
		{"osc-host-port", 	required_argument, 0, 'h'},
		{"osc-listen-port",	required_argument, 0, 'l'},
		{0, 0, 0, 0}
	};
	
	memset(&state, 0, sizeof(rove_state_t));
	
	cols            = 0;
	session_file    = NULL;
	osc_prefix      = NULL;
	osc_host_port   = NULL;
	osc_listen_port = NULL;
	
	opterr = 0;
	
	while( (c = getopt_long(argc, argv, "c:p:h:l:", arguments, &i)) > 0 ) {
		switch( c ) {
		case 'c':
			cols = ((atoi(optarg) - 1) & 0xF) + 1;
			
			if( !cols )
				cols = 8;
			
			break;
			
		case 'p':
			if( *optarg == '/' ) /* remove the leading slash if there is one */
				optarg++;
			
			osc_prefix = strdup(optarg);
			
			break;
			
		case 'h':
			osc_host_port = strdup(optarg);

			do {
				if( !is_numeric(*optarg) ) {
					usage();
					printf("\nerror: \"%s\" is not a valid host port.\n\n", osc_host_port);
					return 1;
				}
			} while( *++optarg );

			break;
			
		case 'l':
			osc_listen_port = strdup(optarg);

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
	
	state.files    = rove_list_new();
	state.patterns = rove_list_new();

	state.active   = rove_list_new();
	state.staging  = rove_list_new();
	
	pthread_mutex_init(&state.monome_mutex, NULL);
	pthread_cond_init(&state.monome_display_notification, NULL);

	printf("you've got the following loops loaded:\n"
		   "\t[rows]\t[file]\n");
	
	if( load_session_file(session_file, &state, &cols) ) {
		printf("error parsing session file :(\n");
		return 1;
	}
	
	if( load_user_conf(&cols, &osc_prefix, &osc_host_port, &osc_listen_port) )
		return 1;
		
	if( rove_list_is_empty(state.files) ) {
		fprintf(stderr, "\t(none, evidently.  get some and come play!)\n\n");
		return 1;
	}
	
	if( rove_jack_init(&state) )
		return 1;
	
	rove_recalculate_bpm_variables(&state);
	state.frames = state.snap_delay;
	
	if( rove_monome_init(&state,
						 (osc_prefix) ? osc_prefix : DEFAULT_OSC_PREFIX,
						 (osc_host_port) ? osc_host_port : DEFAULT_OSC_HOST_PORT,
						 (osc_listen_port) ? osc_listen_port : DEFAULT_OSC_LISTEN_PORT,
						 (cols) ? cols : DEFAULT_MONOME_COLUMNS) )
		return 1;
	
	if( osc_prefix )
		free(osc_prefix);

	if( osc_host_port )
		free(osc_host_port);
	
	if( osc_listen_port )
		free(osc_listen_port);
	
	if( rove_jack_activate(&state) )
		return 1;
	
	rove_monome_run_thread(state.monome);
	main_loop(&state);

	return 0;
}
