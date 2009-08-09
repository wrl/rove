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

#include "rove_list.h"

rove_list_t *rove_list_new() {
	rove_list_t *self;
	
	if( !(self = calloc(1, sizeof(rove_list_t))) )
		goto err_list;

	if( !(self->head = calloc(1, sizeof(rove_list_member_t))) )
		goto err_list_head;
	
	if( !(self->tail = calloc(1, sizeof(rove_list_member_t))) )
		goto err_list_tail;
	
	self->head->prev = NULL;
	self->head->next = self->tail;
	self->tail->prev = self->head;
	self->tail->next = NULL;
	
	return self;

 err_list_tail:
	free(self->head);
 err_list_head:
	free(self);
 err_list:
	return NULL;
}

void rove_list_free(rove_list_t *self) {
	free(self->head);
	free(self->tail);
	free(self);
}

rove_list_member_t *rove_list_push(rove_list_t *self, rove_list_global_location_t l, void *data) {
	rove_list_member_t *m = calloc(1, sizeof(rove_list_member_t));
	
	if( !m )
		return NULL;
	
	m->data = data;
	
	if( l == HEAD ) {
		m->prev = self->head;
		m->next = self->head->next;
		
		self->head->next->prev = m;
		self->head->next = m;
	} else {
		m->prev = self->tail->prev;
		m->next = self->tail;
		
		self->tail->prev->next = m;
		self->tail->prev = m;
	}
	
	return m;
}

rove_list_member_t *rove_list_insert(void *data, rove_list_local_location_t l, rove_list_member_t *rel) {
	rove_list_member_t *m = calloc(1, sizeof(rove_list_member_t));
	
	if( !m )
		return NULL;
	
	m->data = data;
	
	if( l == AFTER ) {
		m->prev = rel;
		m->next = rel->next;
		
		rel->next->prev = m;
		rel->next = m;
	} else {
		m->prev = rel->prev;
		m->next = rel;
		
		rel->prev->next = rel;
		rel->prev = rel;
	}
	
	return m;
}

void *rove_list_pop(rove_list_t *self, rove_list_global_location_t l) {
	rove_list_member_t *m;
	void *data;
	
	if( l == HEAD ) {
		m = self->head->next;
		
		if( !m->next )
			return NULL;
		
		data = m->data;
		
		m->next->prev    = self->head;
		self->head->next = m->next;
	} else {
		m = self->tail->prev;
		
		if( !m->prev )
			return NULL;
		
		data = m->data;
		
		m->prev->next    = self->tail;
		self->tail->prev = m->prev;
	}
	
	free(m);
	return data;
}

void *rove_list_remove(rove_list_t *self, rove_list_member_t *m) {
	void *data = m->data;

	if( !m->next || !m->prev )
		return NULL;
	
	m->prev->next = m->next;
	m->next->prev = m->prev;
	free(m);
	
	return data;
}
