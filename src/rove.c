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

#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rove.h"
#include "rove_file.h"
#include "rove_jack.h"
#include "rove_list.h"
#include "rove_config.h"
#include "rove_monome.h"

#define DEFAULT_CONF_FILE_NAME  ".rove.conf"

#define DEFAULT_MONOME_COLUMNS  8
#define DEFAULT_MONOME_ROWS     8

#define DEFAULT_OSC_PREFIX      "rove"
#define DEFAULT_OSC_HOST_PORT   "8080"
#define DEFAULT_OSC_LISTEN_PORT "8000"

#define MAX_LENGTH 1024

#define usage_printf_exit(...)		do { usage(); printf(__VA_ARGS__); exit(EXIT_FAILURE); } while(0);
#define usage_printf_return(...)	do { usage(); printf(__VA_ARGS__); return 1;           } while(0);

static char *osc_prefix, *osc_host_port, *osc_listen_port;
static int cols, rows, session_cols;

rove_state_t state;

static int is_numstr(char *str) {
	/* scan through the string looking for either a NULL (end of string)
	   or a non-numeric character */

	/* 48 is ASCII '0', 57 is ASCII '9'. tests one character. */
#define is_numeric(c) ((48 <= c) && (c <= 57))

	for(; *str && is_numeric(*str); str++);

	/* did we make it all the way through? */
	if( *str )
		return 0;

	return 1;
}

static rove_group_t *initialize_groups(const int group_count) {
	rove_group_t *groups;
	int i;
	
	if( !(groups = calloc(sizeof(rove_group_t), group_count)) )
		return NULL;

	for( i = 0; i < group_count; i++ )
		groups[i].idx = i;
	
	return groups;
}

static void usage() {
	printf("Usage: rove [OPTION]... session_file.rv\n"
		   "  -c, --monome-columns=COLUMNS\n"
		   "  -r, --monome-rows=ROWS\n"
		   "\n"
		   "  -p, --osc-prefix=PREFIX\n"
		   "  -h, --osc-host-port=PORT\n"
		   "  -l, --osc-listen-port=PORT\n\n");
}

static void monome_display_loop(const rove_state_t *state) {
	int j, group_count, next_bit;
	uint16_t dfield;

	struct timespec req;

	rove_monome_t *monome = state->monome;
	rove_group_t *g;
	rove_file_t *f;

	req.tv_sec  = 0;
	req.tv_nsec = 1000000000 / 80; /* 80 fps */
	
	for(;;) {
		group_count = state->group_count;

		for( j = 0; j < group_count; j++ ) {
			g = &state->groups[j];
			f = g->active_loop;

			if( !f )
				continue;

			rove_monome_display_file(f);
		}

		dfield = monome->dirty_field >> 1;

		for( j = 0; dfield; dfield >>= next_bit ) {
			next_bit = ffs(dfield);
			j += next_bit;

			f = (rove_file_t *) state->monome->callbacks[j].data;
			rove_monome_display_file(f);
		}

		nanosleep(&req, NULL);
	}
}

static char *user_config_path() {
	char *home, *path;
	
	if( !(home = getenv("HOME")) )
		return NULL;
	
	asprintf(&path, "%s/%s", home, DEFAULT_CONF_FILE_NAME);
	return path;
}

static void file_section_callback(const rove_config_section_t *section, void *arg) {
	rove_state_t *state = arg;
	static unsigned int y = 1;

	unsigned int c, r, group, reverse, *v;
	rove_file_t *f;
	double speed;
	char *path;
	
	rove_config_pair_t *pair = NULL;
	
	path    = NULL;
	group   = 0;
	r       = 1;
	c       = 0;
	reverse = 0;
	speed   = 1.0;
	
	while( (c = rove_config_getvar(section, &pair)) ) {
		switch( c ) {
		case 'p':
			path = pair->value;
			continue;
			
		case 'v':
			reverse = 1;
			continue;

		case 's':
			speed = strtod(pair->value, NULL);
			continue;

		case 'g':
			v = &group;
			break;
			
		case 'c':
			v = &c;
			break;

		case 'r':
			v = &r;
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
	f->speed = speed;
	f->row_span = r;
	f->columns  = (c) ? ((c - 1) & 0xF) + 1 : session_cols;
	f->group = &state->groups[group - 1];
	f->play_direction = ( reverse ) ? FILE_PLAY_DIRECTION_REVERSE : FILE_PLAY_DIRECTION_FORWARD;
	
	rove_list_push(state->files, TAIL, f);
	printf("\t%d - %d\t%s\n", y, y + r - 1, path);
	
	y += r;
	
 out:
	free(path);
	return;
}

static void session_section_callback(const rove_config_section_t *section, void *arg) {
	rove_state_t *state = arg;
	
	rove_config_default_section_callback(section);
	state->groups = initialize_groups(state->group_count); /* FIXME: can return null, handle properly */
}

static int load_session_file(const char *path, rove_state_t *state) {
	rove_config_var_t file_vars[] = {
		{"path",    NULL, STRING, 'p'},
		{"groups",  NULL,    INT, 'g'},
		{"columns", NULL,    INT, 'c'},
		{"rows",    NULL,    INT, 'r'},
		{"reverse", NULL,   BOOL, 'v'},
		{"speed",   NULL, DOUBLE, 's'},
		{NULL}
	};
	
	rove_config_var_t session_vars[] = {
		{"quantize", &state->beat_multiplier, DOUBLE, 'q'},
		{"bpm",      &state->bpm,             DOUBLE, 'b'},
		{"groups",   &state->group_count,        INT, 'g'},
		{"pattern1", &state->pattern_lengths[0], INT, '1'},
		{"pattern2", &state->pattern_lengths[1], INT, '2'},
		{"columns",  &session_cols,              INT, 'c'},
		{NULL}
	};
	
	rove_config_section_t config_sections[] = {
		{"session", session_vars, session_section_callback, state},
		{"file",    file_vars   , file_section_callback,    state},
		{NULL}
	};
	
	session_cols = 0;

	if( rove_load_config(path, config_sections, 1) )
		return 1;
	
	return 0;
}

static int load_user_conf() {
	char *op, *ohp, *olp, *conf;
	int c, r;

	rove_config_var_t monome_vars[] = {
		{"columns", &c, INT, 'c'},
		{"rows",    &r, INT, 'r'},
		{NULL}
	};
	
	rove_config_var_t osc_vars[] = {
		{"prefix",      &op,  STRING, 'p'},
		{"host-port",   &ohp, STRING, 'h'},
		{"listen-port", &olp, STRING, 'l'},
		{NULL}
	};
	
	rove_config_section_t config_sections[] = {
		{"monome", monome_vars},
		{"osc",    osc_vars},
		{NULL}
	};
	
	c   = 0;
	r   = 0;
	op  = NULL;
	ohp = NULL;
	olp = NULL;

	if( !(conf = user_config_path()) )
		return 0;
	
	if( rove_load_config(conf, config_sections, 0) )
		return 0;
	
	free(conf);
	
	if( c && !cols )
		cols = ((c - 1) & 0xF) + 1;

	if( r && !rows )
		rows = ((r - 1) & 0xF) + 1;
	
	if( op && !osc_prefix ) {
		if( *op == '/' ) { /* remove the leading slash if there is one */
			conf = strdup(op + 1);
			free(op);
			op = conf;
		}
		
		osc_prefix = op;
	}
	
	if( ohp && !osc_host_port ) {
		if( !is_numstr(ohp) )
			usage_printf_return("rove_config: \"%s\" is not a valid host port.\n"
								"             please check your conf file!\n", ohp);

		osc_host_port = ohp;
	}
	
	if( olp && !osc_listen_port ) {
		if( !is_numstr(olp) )
			usage_printf_return("rove_config: \"%s\" is not a valid listen port.\n"
								"             please check your conf file!\n", ohp);

		osc_listen_port = olp;
	}

	return 0;
}

static void rove_recalculate_bpm_variables(rove_state_t *state) {
	state->snap_delay = lrint(((60 / state->bpm) * state->beat_multiplier) * ((double) jack_get_sample_rate(state->client)));
	
	if( !state->snap_delay )
		state->snap_delay++;
}

static void exit_on_signal(int s) {
	exit(0);
}

static void cleanup() {
	rove_monome_stop_thread(state.monome);
	rove_monome_free(state.monome);
	
	rove_jack_deactivate(&state);
}

int main(int argc, char **argv) {
	char *session_file, c;
	int i;
	
	struct option arguments[] = {
		{"help",			no_argument,       0, 'u'}, /* for "usage", get it?  hah, hah... */
		{"monome-columns",	required_argument, 0, 'c'},
		{"monome-rows",		required_argument, 0, 'r'},
		{"osc-prefix", 		required_argument, 0, 'p'},
		{"osc-host-port", 	required_argument, 0, 'h'},
		{"osc-listen-port",	required_argument, 0, 'l'},
		{0, 0, 0, 0}
	};
	
	memset(&state, 0, sizeof(rove_state_t));
	
	cols            = 0;
	rows            = 0;
	session_file    = NULL;
	osc_prefix      = NULL;
	osc_host_port   = NULL;
	osc_listen_port = NULL;
	
	opterr = 0;
	
	while( (c = getopt_long(argc, argv, "uc:r:p:h:l:", arguments, &i)) > 0 ) {
		switch( c ) {
		case 'u':
			usage();
			exit(EXIT_FAILURE); /* should we exit after displaying usage? (i think so) */

		case 'c':
			cols = ((atoi(optarg) - 1) & 0xF) + 1;
			break;
			
		case 'r':
			rows = ((atoi(optarg) - 1) & 0xF) + 1;
			break;
			
		case 'p':
			if( *optarg == '/' ) /* remove the leading slash if there is one */
				optarg++;
			
			osc_prefix = strdup(optarg);
			break;
			
		case 'h':
			if( !is_numstr(optarg) )
				usage_printf_exit("error: \"%s\" is not a valid host port.\n\n", optarg);

			osc_host_port = strdup(optarg);
			break;
			
		case 'l':
			if( !is_numstr(optarg) )
				usage_printf_exit("error: \"%s\" is not a valid listen port.\n\n", optarg);

			osc_listen_port = strdup(optarg);
			break;
		}
	}
	
	if( optind == argc )
		usage_printf_exit("error: you did not specify a session file!\n\n");
	
	session_file = argv[optind];

	state.files    = rove_list_new();
	state.patterns = rove_list_new();

	state.active   = rove_list_new();
	state.staging  = rove_list_new();
	
	pthread_mutex_init(&state.monome_mutex, NULL);
	pthread_cond_init(&state.monome_display_notification, NULL);

	if( load_user_conf() )
		exit(EXIT_FAILURE);
		
	printf("\nhey, welcome to rove!\n\n"
		   "you've got the following loops loaded:\n"
		   "\t[rows]\t[file]\n");
	
	if( load_session_file(session_file, &state) ) {
		printf("error parsing session file :(\n");
		exit(EXIT_FAILURE);
	}
	
	if( rove_list_is_empty(state.files) ) {
		fprintf(stderr, "\t(none, evidently.  get some and come play!)\n\n");
		exit(EXIT_FAILURE);
	}
	
	if( rove_jack_init(&state) ) {
		fprintf(stderr, "error initializing JACK :(\n");
		exit(EXIT_FAILURE);
	}
	
	rove_recalculate_bpm_variables(&state);
	state.frames = state.snap_delay;
	
	if( rove_monome_init(&state,
						 (osc_prefix) ? osc_prefix : DEFAULT_OSC_PREFIX,
						 (osc_host_port) ? osc_host_port : DEFAULT_OSC_HOST_PORT,
						 (osc_listen_port) ? osc_listen_port : DEFAULT_OSC_LISTEN_PORT,
						 (cols) ? cols : DEFAULT_MONOME_COLUMNS,
						 (rows) ? rows : DEFAULT_MONOME_ROWS) )
		exit(EXIT_FAILURE);

	if( osc_prefix )
		free(osc_prefix);

	if( osc_host_port )
		free(osc_host_port);
	
	if( osc_listen_port )
		free(osc_listen_port);
	
	if( rove_jack_activate(&state) )
		exit(EXIT_FAILURE);
	
	signal(SIGINT, exit_on_signal);
	atexit(cleanup);
	
	rove_monome_run_thread(state.monome);
	monome_display_loop(&state);

	return 0;
}
