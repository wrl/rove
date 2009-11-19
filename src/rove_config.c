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

#include <libgen.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#include "rove_config.h"
#include "rove_list.h"

#define BUFLEN 256

#define COMMENT_SYMBOL     '#'
#define BLOCK_OPEN_SYMBOL  '['
#define BLOCK_CLOSE_SYMBOL ']'

typedef enum {
	COMMENT,
	KEY,
	VALUE,
	BLOCK
} parse_mode_t;

int rove_config_getvar(const rove_config_section_t *section, rove_config_pair_t **current_pair) {
	const rove_config_var_t *v;
	rove_config_pair_t *p;
	int i;
	
	/* if we're called incrementally, free the previous key->value pair */
	if( *current_pair ) {
		p = *current_pair;
		
		/* assume that other code holds reference to the value */
		if( p->var->type != STRING )
			free(p->value);
		
		free(p->key);
		free(p);
	}
	
	while( (p = rove_list_pop(section->pairs, HEAD)) ) {
		/* search for a matching var structure */
		for( i = 0; section->vars[i].key; i++ ) {
			v = &section->vars[i];
			
			if( !strncmp(p->key, v->key, p->klen) ) {
				p->var = v;
				*current_pair = p;

				return v->val;
			}
		}
		
		/* variable in conf file that was not in the array of vars */
		free(p->value);
		free(p->key);
		free(p);
	}
	
	return 0;
}

void rove_config_default_section_callback(const rove_config_section_t *section) {
	rove_config_pair_t *p;
	const rove_config_var_t *v;
	int i;
	
	if( !section->vars )
		return;

	while( (p = rove_list_pop(section->pairs, HEAD)) ) {
		/* search for a matching var structure */
		for( i = 0; section->vars[i].key; i++ ) {
			v = &section->vars[i];
			p->var = v;
			
			if( !strncmp(p->key, v->key, p->klen) ) {
				switch( v->type ) {
				case STRING:
					if( p->value )
						*((char **) v->dest) = p->value;
					goto assigned;
					
				case INT:
					if( p->value )
						*((int *) v->dest) = (int) strtol(p->value, NULL, 10);
					break;
					
				case LONG:
					if( p->value )
						*((long int *) v->dest) = strtol(p->value, NULL, 10);
					break;
					
				case DOUBLE:
					if( p->value )
						*((double *) v->dest) = strtod(p->value, NULL);
					break;
					
				case BOOL:
					*((int *) v->dest) = (p->vlen) ? 1 : 0;
					break;
				}
				
				break;
			}
		}
		
		/* if the variable is a string, assume that other code holds reference to the value */
		free(p->value);
	assigned:
		free(p->key);
		free(p);
	}
}

static void close_block(rove_config_section_t *s, rove_config_pair_t *p) {
	if( p )
		rove_list_push(s->pairs, HEAD, p);

	if( s ) {
		if( s->section_callback )
			s->section_callback(s, s->cb_arg);
		else
			rove_config_default_section_callback(s);
		
		rove_list_free(s->pairs);
		s->pairs = NULL;
	}
}


static int config_parse(const char *path, rove_config_section_t *sections, int cd) {
	char buf[BUFLEN + 1], vbuf[BUFLEN + 1], c;
	rove_config_section_t *s = NULL;
	int fd, len, vlen, i, j, lines;
	rove_config_pair_t *p = NULL;
	parse_mode_t m = KEY;
	
	if( (fd = open(path, O_RDONLY)) < 0 )
		return 1;
	
	if( cd ) 
		chdir(dirname((char *) path));
	
	vlen = 0;
	lines = 1;
	
	while( (len = read(fd, buf, BUFLEN)) ) {
		buf[len] = '\0';

		if( len < BUFLEN )
			len++;

		for( i = 0; i < len; i++ ) {
			c = buf[i];

			if( !c ) {
				lines++;
				goto state_out;
			}

			switch( m ) {
			case COMMENT:
				if( c == '\n' )
					goto state_out;

				break;
				
			case KEY:
				switch( c ) {
				case ' ':
					continue;
					
				case COMMENT_SYMBOL:
				case '\n':
				case '=':
					goto state_out;
					
				case BLOCK_OPEN_SYMBOL:
					m = BLOCK;
					continue;
				}
				
				vbuf[vlen++] = c;
				
				break;
				
			case BLOCK:
				switch( c ) {
				case BLOCK_CLOSE_SYMBOL:
				case COMMENT_SYMBOL:
				case '\n':
					goto state_out;
				}
				
				vbuf[vlen++] = c;
				
				break;
				
			case VALUE:
				switch( c ) {
				case ' ':
					if( !vlen )
						continue;
					break;
					
				case COMMENT_SYMBOL:
				case '\n':
					goto state_out;
				}
				
				vbuf[vlen++] = c;
			}
			
			continue;

		state_out:
			if( c == '\n' )
				lines++;
			
			vbuf[vlen] = 0;
			
			switch( m ) {
			case BLOCK:
				if( c != BLOCK_CLOSE_SYMBOL ) {
					vbuf[vlen] = 0;
					printf("rove_config: unterminated block name \"%s\" on line %d of %s\n", vbuf, lines, path);
					return 1;
				}

				close_block(s, p);

				s = NULL;
				p = NULL;

				for( j = 0; sections[j].block; j++ )
					if( !strncmp(vbuf, sections[j].block, vlen) ) {
						s = &sections[j];
						s->start_line = lines;

						if( !s->pairs )
							s->pairs = rove_list_new();
					}

				m = COMMENT;
				break;

			case COMMENT:
				m = KEY;
				continue;

			case KEY:
				switch( c ) {
				case '=':
					if( !vlen ) {
						printf("rove_config: missing key on line %d of %s\n", lines, path);
						return 1;
					}

					m = VALUE;
					/* fall through */

				case '\n':
					if( !vlen )
						continue;
					break;

				case COMMENT_SYMBOL:
					m = COMMENT;

					if( !vlen )
						continue;
				}

				if( s ) {
					if( p )
						rove_list_push(s->pairs, HEAD, p);

					p = calloc(1, sizeof(rove_config_pair_t));
					p->key  = strndup(vbuf, vlen);
					p->klen = vlen;
				}

				break;

			case VALUE:
				if( vbuf[vlen - 1] == ' ' ) {
					while( vbuf[vlen - 1] == ' ' ) /* strip trailing spaces */
						vlen--;

					vbuf[vlen] = 0;
				}

				m = ( c == COMMENT_SYMBOL ) ? COMMENT : KEY;

				if( p ) {
					p->value = strndup(vbuf, vlen);
					p->vlen  = vlen;
				}

				break;
			}
			
			vlen = 0;
		}
	}
	
	close_block(s, p);
	return 0;
}

int rove_load_config(const char *path, rove_config_section_t *sections, int cd) {
	int i;
	
	for( i = 0; sections[i].block; i++ )
		sections[i].pairs = NULL;

	return config_parse(path, sections, cd);
}
