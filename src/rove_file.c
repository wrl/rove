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

#include <sndfile.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_SRC
#include <samplerate.h>
#endif

#include "rove_types.h"
#include "rove_file.h"

static void rove_file_init(rove_file_t *f) {
	f->state = FILE_STATE_INACTIVE;
	f->play_direction = FILE_PLAY_DIRECTION_FORWARD;
	f->volume = 1.0;
}

long rove_file_src_callback(void *cb_data, float **data) {
	rove_file_t *f = cb_data;
	sf_count_t o;
	
	if( !data )
		return 0;
	
	o = f->play_offset;
	*data = f->file_data + (o * f->channels);
	rove_file_inc_play_pos(f, 1);
	
	return 1;
}

void rove_file_free(rove_file_t *f) {
	free(f->file_data);
	free(f);
	
	f = NULL;
}
		 
rove_file_t *rove_file_new_from_path(const char *path) {
#ifdef HAVE_SRC
	int err;
#endif
	rove_file_t *f;
	SF_INFO info;
	SNDFILE *snd;
	
	if( !(f = calloc(sizeof(rove_file_t), 1)) )
		return NULL;
	
	rove_file_init(f);
	
	if( !(snd = sf_open(path, SFM_READ, &info)) ) {
		printf("file: couldn't load \"%s\".  sorry about your luck.\n%s\n\n", path, sf_strerror(snd));
		
		free(f);
		return NULL;
	}
	
	f->length      = f->file_length = info.frames;
	f->channels    = info.channels;
	f->sample_rate = info.samplerate;
	f->file_data   = calloc(sizeof(float), info.frames * info.channels);
	
#ifdef HAVE_SRC
	f->src         = src_callback_new(rove_file_src_callback, SRC_SINC_FASTEST, info.channels, &err, f);
#endif
	
	if( sf_readf_float(snd, f->file_data, f->length) != f->length )
		rove_file_free(f);
	
	sf_close(snd);
	
	return f;
}

void rove_file_set_play_pos(rove_file_t *f, sf_count_t p) {
	if( p >= f->file_length )
	   p -= f->file_length * (abs(p) / f->file_length);
	
	if( p < 0 )
		p += f->file_length * (1 + (abs(p) / f->file_length));
	
	f->play_offset = p;
	return;
}

void rove_file_inc_play_pos(rove_file_t *f, sf_count_t delta) {
	if( f->play_direction == FILE_PLAY_DIRECTION_REVERSE )
		rove_file_set_play_pos(f, f->play_offset - delta);
	else
		rove_file_set_play_pos(f, f->play_offset + delta);
}

void rove_file_activate(rove_file_t *f) {
	if( f->group->active_loop )
		if( f->group->active_loop != f )
			if( rove_file_is_active(f->group->active_loop) )
				f->group->active_loop->state = FILE_STATE_DEACTIVATE;
	
	f->group->active_loop = f;
	f->state = FILE_STATE_ACTIVE;
	
	return;
}

void rove_file_deactivate(rove_file_t *f) {
	if( f->group->active_loop == f )
		f->group->active_loop = NULL;
	
	f->state = FILE_STATE_INACTIVE;
}

void rove_file_reseek(rove_file_t *f, jack_nframes_t offset) {
	rove_file_set_play_pos(f, f->new_offset + offset);
	f->state = FILE_STATE_ACTIVE;
}
