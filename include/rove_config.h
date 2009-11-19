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

#include "rove_list.h"

typedef enum {
	INT,
	LONG,
	STRING,
	DOUBLE,
	BOOL
} rove_config_var_type_t;

typedef struct rove_config_pair rove_config_pair_t;
typedef struct rove_config_section rove_config_section_t;
typedef struct rove_config_var rove_config_var_t;

typedef void (*rove_config_section_callback_t)(const rove_config_section_t *, void *arg);

struct rove_config_var {
	const char *key;
	void *dest;
	rove_config_var_type_t type;
	int val;
};

struct rove_config_pair {
	char *key;
	char *value;
	
	const rove_config_var_t *var;
	int klen;
	int vlen;
};

struct rove_config_section {
	char *block;
	const rove_config_var_t *vars;
	rove_config_section_callback_t section_callback;
	void *cb_arg;

	int start_line;
	rove_list_t *pairs;
};

int rove_load_config(const char *path, rove_config_section_t *sections, int cd);
void rove_config_default_section_callback(const rove_config_section_t *section);
int rove_config_getvar(const rove_config_section_t *section, rove_config_pair_t **current_pair);
