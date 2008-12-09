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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#include "rove_types.h"
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

static void generic_section_callback(const rove_config_section_t *section) {
	rove_config_pair_t *p;
	const rove_config_var_t *v;
	int i;
	
	if( !section->vars )
		return;

	while( (p = rove_list_pop(section->pairs, HEAD)) ) {
		for( i = 0; section->vars[i].key; i++ ) {
			v = &section->vars[i];
			
			if( !strncmp(p->key, v->key, p->klen) ) {
				printf("%s: '%s'\n", v->key, p->value);
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
		
		free(p->value);
	assigned:
		free(p->key);
		free(p);
	}
	
	rove_list_free(section->pairs);
}

static void close_block(const rove_config_section_t *s, rove_config_pair_t *p) {
	if( p )
		rove_list_push(s->pairs, HEAD, p);

	if( s ) {
		if( s->section_callback )
			s->section_callback(s, s->cb_arg);
		else
			generic_section_callback(s);
	}
}


static int config_parse(const char *path, const rove_config_section_t *sections) {
	char buf[BUFLEN + 1], vbuf[BUFLEN + 1], c;
	const rove_config_section_t *s = NULL;
	int fd, len, vlen, i, j, lines;
	rove_config_pair_t *p = NULL;
	parse_mode_t m = KEY;
	
	if( (fd = open(path, O_RDONLY)) < 0 )
		return 1;
	
	vlen = 0;
	lines = 1;
	
	while( (len = read(fd, buf, BUFLEN)) ) {
		if( len < BUFLEN ) {
			if( buf[len] != '\n' )
				buf[len++] = '\n';
		}
		
		for( i = 0; i < len; i++ ) {
			c = buf[i];
			
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
				case '=':
				case '\n':
					goto state_out;
					
				case BLOCK_OPEN_SYMBOL:
					m = BLOCK;
					continue;
				}
				
				vbuf[vlen++] = c;
				
				break;
				
			case BLOCK:
				switch( c ) {
				case COMMENT_SYMBOL:
				case '\n':
					vbuf[vlen] = 0;
					printf("rove_config: unterminated block name \"%s\" on line %d of %s\n", vbuf, lines, path);
					return 1;
					
				case BLOCK_CLOSE_SYMBOL:
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
			if( c == '\n')
				lines++;
			
			vbuf[vlen] = 0;
			
			switch( m ) {
			case BLOCK:
				close_block(s, p);
				
				s = NULL;
				p = NULL;
				
				for( j = 0; sections[j].block; j++ )
					if( !strncmp(vbuf, sections[j].block, vlen) )
						s = &sections[j];
				
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

int rove_load_config(const char *path, rove_config_section_t *sections) {
	int i;
	
	for( i = 0; sections[i].block; i++ )
		sections[i].pairs = rove_list_new();
	
	return config_parse(path, sections);
}
