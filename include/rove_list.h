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

#include "rove_types.h"

#define rove_list_foreach(list, cursor, datum) for( cursor = list->head->next, datum = cursor->data;\
													cursor->next; cursor = cursor->next, datum = cursor->data )
#define rove_list_is_empty(list) (list->head->next == list->tail && list->tail->prev == list->head)

rove_list_t *rove_list_new();
void  rove_list_free(rove_list_t *list);

void  rove_list_push(rove_list_t *list, rove_list_global_location_t l, void *data);
void *rove_list_pop(rove_list_t *list, rove_list_global_location_t l);
void  rove_list_remove(rove_list_t *list, rove_list_member_t *m);

#endif
