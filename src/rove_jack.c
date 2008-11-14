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

#include <pthread.h>
#include <samplerate.h>
#include <sndfile.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>

#include "rove_file.h"
#include "rove_jack.h"
#include "rove_list.h"
#include "rove_monome.h"
#include "rove_pattern.h"

static jack_port_t *outport_l;
static jack_port_t *outport_r;

#define SRATE_ROUND 100

static int process(jack_nframes_t nframes, void *arg) {
	rove_state_t *state = arg;
	
	jack_default_audio_sample_t *out_l = jack_port_get_buffer(outport_l, nframes);
	jack_default_audio_sample_t *out_r = jack_port_get_buffer(outport_r, nframes);
	
	sf_count_t i, o;
	
	rove_file_t *f;
	rove_pattern_t *p;
	rove_list_member_t *m;
	rove_pattern_step_t *s;
	
	for( i = 0; i < nframes; i++ )
		out_l[i] = out_r[i] = 0;
	
	for( i = 0; i < nframes; i++ ) {
		if( state->snap_delay > 0 )
			if( ++state->frames > state->snap_delay )
				state->frames = 0;

		rove_list_foreach(state->files, m, f) {
			o = rove_file_get_play_pos(f);
			
			switch( f->state ) {
			case FILE_STATE_DEACTIVATE:
			case FILE_STATE_INACTIVE:
				continue;
				
			case FILE_STATE_ACTIVATE:
				if( !state->frames )
					rove_file_activate(f);
				else
					continue;
				
				break;

			case FILE_STATE_RESEEK:
				if( !state->frames ) {
					rove_file_reseek(f, state->frames);
					o = rove_file_get_play_pos(f);
				}

				break;
				
			case FILE_STATE_ACTIVE:
				break;
			}
			
			if( f->channels == 1 ) {
				out_l[i] += f->file_data[o]   * f->volume;
				out_r[i] += f->file_data[o]   * f->volume;
			} else {
				out_l[i] += f->file_data[o]   * f->volume;
				out_r[i] += f->file_data[++o] * f->volume;
			}
			
			rove_file_inc_play_pos(f, 1);
		}
		
		rove_list_foreach(state->patterns, m, p) {
			switch( p->status ) {
			case PATTERN_STATUS_ACTIVATE:
				if( !state->frames ) {
					p->status = PATTERN_STATUS_ACTIVE;
					p->current_step = p->steps->tail->prev;
					p->delay_frames = ((rove_pattern_step_t *) p->current_step->data)->delay;
				} else
					break;
				
			case PATTERN_STATUS_ACTIVE:
				s = p->current_step->data;
				
				if( p->delay_frames >= s->delay ) {
					p->current_step = p->current_step->next;

					if( !p->current_step->next )
						p->current_step = p->steps->head->next;
					
					p->delay_frames = 0;
					s = p->current_step->data;
					
					switch( s->cmd ) {
					case CMD_GROUP_DEACTIVATE:
						s->file->state = FILE_STATE_DEACTIVATE;
						break;

					case CMD_LOOP_SEEK:
						if( !rove_file_is_active(s->file) ) {
							s->file->play_offset = s->arg;
							s->file->new_offset  = -1;
							
							s->file->force_monome_update = 1;
							rove_file_activate(s->file);
						} else {
							s->file->new_offset = s->arg;
							rove_file_reseek(s->file, 0);
						}

						break;
					}
				}
				
				p->delay_frames++;
				
				break;
				
			case PATTERN_STATUS_RECORDING:
				if( !state->frames ) {
					p = state->pattern_rec->data;
					if( !rove_list_is_empty(p->steps) ) {
						((rove_pattern_step_t *) p->steps->tail->prev->data)->delay += (state->snap_delay) ? state->snap_delay : 1;
					
						if( p->delay_frames ) {
							if( (p->delay_frames -= state->snap_delay) <= 0 ) {
								p->status = PATTERN_STATUS_ACTIVATE;
								state->pattern_rec = NULL;
							}
						}
					}
				}

				break;

			default:
				break;
			}
		}		

		pthread_cond_broadcast(&state->monome_display_notification);
	}
	
	return 0;
}

static void jack_shutdown(void *arg) {
	exit(0);
}

static void connect_to_outports(jack_client_t *client) {
	const char **ports;
	
	ports = jack_get_ports(client, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
	if( ports != NULL ) {
		jack_connect(client, jack_port_name(outport_l), ports[0]);
		jack_connect(client, jack_port_name(outport_r), ports[1]);
	}
	
	free(ports);
}

void rove_transport_start(rove_state_t *state) {
	jack_transport_start(state->client);
}

void rove_transport_stop(rove_state_t *state) {
	jack_transport_stop(state->client);
	jack_transport_locate(state->client, 0);
}

int rove_jack_activate(rove_state_t *state) {
	if( jack_activate(state->client) ) {
		fprintf(stderr, "client could not be activated\n");
		return -1;
	}
	
	connect_to_outports(state->client);
	
	return 0;
}

int rove_jack_init(rove_state_t *state) {
	const char *client_name = "rove";
	const char *server_name = NULL;
	jack_options_t options  = JackNoStartServer;
	jack_status_t status;
	
	state->client = jack_client_open(client_name, options, &status, server_name);
	if( state->client == NULL ) {
		fprintf(stderr, "failed to open a connection to the JACK server\n");
		return -1;
	}

	jack_set_process_callback(state->client, process, state);
	jack_on_shutdown(state->client, jack_shutdown, 0);

	outport_l = jack_port_register(state->client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	outport_r = jack_port_register(state->client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	
	return 0;
}
