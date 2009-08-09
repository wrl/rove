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

#include "rove_types.h"
#include "rove_file.h"

void rove_group_activate_file(rove_file_t *file) {
	if( file->group->active_loop )
		if( file->group->active_loop != file )
			if( rove_file_is_active(file->group->active_loop) )
				rove_file_deactivate(file->group->active_loop);

	file->group->active_loop = file;
}
