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

#define list_foreach(list, cursor, datum) \
	for( cursor = list->head.next, datum = cursor->data;\
		 cursor->next; cursor = cursor->next, datum = cursor->data )

#define list_foreach_raw(list, cursor) \
	for( cursor = list->head.next; cursor->next; cursor = cursor->next )

#define list_is_empty(list) (list->head.next == &list->tail \
							 && list->tail.prev == &list->head)

#define LIST_T(x) ((list_t *) x)
#define LIST_MEMBER_T(x) ((list_member_t *) x)


typedef enum {
	HEAD,
	TAIL
} list_global_location_t;

typedef enum {
	BEFORE,
	AFTER
} list_local_location_t;

typedef struct list_member list_member_t;
typedef struct list list_t;

struct list_member {
	void *data;

	list_member_t *prev;
	list_member_t *next;
};

struct list {
	list_member_t head;
	list_member_t tail;
};

void list_init(list_t *self);
list_t *list_new();
void  list_free(list_t *list);

/* raw functions do not (de)allocate list_member_t structures,
   non-raw variants do. */

void list_push_raw(list_t *list, list_global_location_t l, list_member_t *m);
list_member_t *list_push(list_t *list, list_global_location_t l, void *data);

void list_insert_raw(list_member_t *m, list_local_location_t l, list_member_t *rel);
list_member_t *list_insert(void *data, list_local_location_t l, list_member_t *rel);

list_member_t *list_pop_raw(list_t *list, list_global_location_t l);
void *list_pop(list_t *list, list_global_location_t l);

int list_remove_raw(list_member_t *m);
void *list_remove(list_member_t *m);

#endif
