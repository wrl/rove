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

#ifndef _ROVE_MONOME_H
#define _ROVE_MONOME_H

#include <lo/lo.h>

#include "rove.h"
#include "rove_file.h"

void monome_led_row_16(rove_monome_t *monome, uint8_t row, uint8_t *row_data);
void monome_clear(rove_monome_t *monome, uint8_t mode);
void monome_led_on(rove_monome_t *monome, uint8_t x, uint8_t y);
void monome_led_off(rove_monome_t *monome, uint8_t x, uint8_t y);

void rove_monome_run_thread(rove_monome_t *monome);
void rove_monome_stop_thread(rove_monome_t *monome);

void rove_monome_blank_file_row(rove_monome_t *monome, rove_file_t *f);
void rove_monome_display_file(rove_state_t *state, rove_file_t *f);
int  rove_monome_init(rove_state_t *state, const char *osc_prefix, const char *osc_host_port, const char *osc_listen_port, const uint8_t cols);

#endif
