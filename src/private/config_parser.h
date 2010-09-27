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

#include "list.h"

typedef enum {
	INT,
	LONG,
	STRING,
	DOUBLE,
	BOOL
} conf_var_type_t;

typedef struct conf_pair conf_pair_t;
typedef struct conf_section conf_section_t;
typedef struct conf_var conf_var_t;

typedef void (*conf_section_callback_t)(const conf_section_t *, void *arg);

struct conf_var {
	const char *key;
	void *dest;
	conf_var_type_t type;
	int val;
};

struct conf_pair {
	char *key;
	char *value;

	const conf_var_t *var;
	int klen;
	int vlen;
};

struct conf_section {
	char *block;
	const conf_var_t *vars;
	conf_section_callback_t section_callback;
	void *cb_arg;

	int start_line;
	list_t *pairs;
};

int conf_load(const char *path, conf_section_t *sections, int cd);
void conf_default_section_callback(const conf_section_t *section);
int conf_getvar(const conf_section_t *section, conf_pair_t **current_pair);
