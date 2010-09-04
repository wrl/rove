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

#ifndef _ROVE_LIST_H
#define _ROVE_LIST_H

#define rove_list_foreach(list, cursor, datum) for( cursor = list->head->next, datum = cursor->data;\
													cursor->next; cursor = cursor->next, datum = cursor->data )
#define rove_list_is_empty(list) (list->head->next == list->tail && list->tail->prev == list->head)

typedef enum {
	HEAD,
	TAIL
} rove_list_global_location_t;

typedef enum {
	BEFORE,
	AFTER
} rove_list_local_location_t;

typedef struct rove_list_member rove_list_member_t;
typedef struct rove_list rove_list_t;

struct rove_list_member {
	void *data;

	rove_list_member_t *prev;
	rove_list_member_t *next;
};

struct rove_list {
	rove_list_member_t *head;
	rove_list_member_t *tail;
};

rove_list_t *rove_list_new();
void  rove_list_free(rove_list_t *list);

/* raw functions do not (de)allocate rove_list_member_t structures,
   non-raw variants do. */

void rove_list_push_raw(rove_list_t *list, rove_list_global_location_t l, rove_list_member_t *m);
rove_list_member_t *rove_list_push(rove_list_t *list, rove_list_global_location_t l, void *data);

void rove_list_insert_raw(rove_list_member_t *m, rove_list_local_location_t l, rove_list_member_t *rel);
rove_list_member_t *rove_list_insert(void *data, rove_list_local_location_t l, rove_list_member_t *rel);

rove_list_member_t *rove_list_pop_raw(rove_list_t *list, rove_list_global_location_t l);
void *rove_list_pop(rove_list_t *list, rove_list_global_location_t l);

int rove_list_remove_raw(rove_list_member_t *m);
void *rove_list_remove(rove_list_member_t *m);

#endif
