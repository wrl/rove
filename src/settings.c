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

#include "config_parser.h"
#include "rove.h"

extern rove_state_t state;

int rove_settings_load(const char *path) {
	char *op, *ohp, *olp, *buf;
	int c, r;

	conf_var_t monome_vars[] = {
		{"columns", &c, INT, 'c'},
		{"rows",    &r, INT, 'r'},
		{NULL}
	};
	
	conf_var_t osc_vars[] = {
		{"prefix",      &op,  STRING, 'p'},
		{"host-port",   &ohp, STRING, 'h'},
		{"listen-port", &olp, STRING, 'l'},
		{NULL}
	};
	
	conf_section_t config_sections[] = {
		{"monome", monome_vars},
		{"osc",    osc_vars},
		{NULL}
	};

	assert(path);
	
	c   = 0;
	r   = 0;
	op  = NULL;
	ohp = NULL;
	olp = NULL;

	if( conf_load(path, config_sections, 0) )
		return 0;
	
	if( c && !state.config.cols )
		state.config.cols = ((c - 1) & 0xF) + 1;

	if( r && !state.config.rows )
		state.config.rows = ((r - 1) & 0xF) + 1;
	
	if( op && !state.config.osc_prefix ) {
		if( *op == '/' ) { /* remove the leading slash if there is one */
			buf = strdup(op + 1);
			free(op);
			op = buf;
		}
		
		state.config.osc_prefix = op;
	}
	
	if( ohp && !state.config.osc_host_port ) {
		if( !is_numstr(ohp) )
			usage_printf_return("conf: \"%s\" is not a valid host port.\n"
								"             please check your conf file!\n", ohp);

		state.config.osc_host_port = ohp;
	}
	
	if( olp && !state.config.osc_listen_port ) {
		if( !is_numstr(olp) )
			usage_printf_return("conf: \"%s\" is not a valid listen port.\n"
								"             please check your conf file!\n", ohp);

		state.config.osc_listen_port = olp;
	}

	return 0;
}

