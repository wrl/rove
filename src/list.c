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

#include "list.h"

list_t *list_new() {
	list_t *self;

	if( !(self = calloc(1, sizeof(list_t))) )
		goto err_list;

	if( !(self->head = calloc(1, sizeof(list_member_t))) )
		goto err_list_head;

	if( !(self->tail = calloc(1, sizeof(list_member_t))) )
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

void list_free(list_t *self) {
	free(self->head);
	free(self->tail);
	free(self);
}

void list_push_raw(list_t *self, list_global_location_t l, list_member_t *m) {
	assert(self);
	assert(m);

	switch( l ) {
	case HEAD:
		m->prev = self->head;
		m->next = self->head->next;

		self->head->next->prev = m;
		self->head->next = m;
		break;

	case TAIL:
		m->prev = self->tail->prev;
		m->next = self->tail;

		self->tail->prev->next = m;
		self->tail->prev = m;
		break;
	}
}

list_member_t *list_push(list_t *self, list_global_location_t l, void *data) {
	list_member_t *m = calloc(1, sizeof(list_member_t));

	if( !m )
		return NULL;

	m->data = data;
	list_push_raw(self, l, m);
	return m;
}

void list_insert_raw(list_member_t *m, list_local_location_t l, list_member_t *rel) {
	assert(m);
	assert(rel);

	switch( l ) {
	case BEFORE:
		m->prev = rel;
		m->next = rel->next;

		rel->next->prev = m;
		rel->next = m;
		break;

	case AFTER:
		m->prev = rel->prev;
		m->next = rel;

		rel->prev->next = rel;
		rel->prev = rel;
		break;
	}
}

list_member_t *list_insert(void *data, list_local_location_t l, list_member_t *rel) {
	list_member_t *m = calloc(1, sizeof(list_member_t));

	if( !m )
		return NULL;

	m->data = data;
	list_insert_raw(m, l, rel);
	return m;
}

list_member_t *list_pop_raw(list_t *self, list_global_location_t l) {
	list_member_t *m;

	assert(self);

	switch( l ) {
	case HEAD:
		m = self->head->next;

		m->next->prev    = self->head;
		self->head->next = m->next;
		break;

	case TAIL:
		m = self->tail->prev;

		m->prev->next    = self->tail;
		self->tail->prev = m->prev;
		break;
	}

	return m;
}

void *list_pop(list_t *self, list_global_location_t l) {
	list_member_t *m;
	void *data;

	assert(self);

	if( list_is_empty(self) )
		return NULL;

	m = list_pop_raw(self, l);
	data = m->data;

	free(m);
	return data;
}

int list_remove_raw(list_member_t *m) {
	assert(m);

	if( !m->next || !m->prev )
		return 1;

	m->prev->next = m->next;
	m->next->prev = m->prev;

	return 0;
}

void *list_remove(list_member_t *m) {
	void *data;

	assert(m);
	data = m->data;

	if( list_remove_raw(m) )
		return NULL;

	free(m);
	return data;
}
