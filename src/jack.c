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

#include <pthread.h>
#include <sndfile.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <jack/jack.h>

#include "file.h"
#include "jack.h"
#include "list.h"
#include "util.h"
#include "rmonome.h"
#include "pattern.h"

extern state_t state;

static jack_port_t *group_mix_inport_l;
static jack_port_t *group_mix_inport_r;

static jack_port_t *outport_l;
static jack_port_t *outport_r;

static void process_file(file_t *f) {
	if( !f )
		return;

	if( f->quantize_cb ) {
		f->quantize_cb(f);
		file_on_quantize(f, NULL);
	}
}

static int process(jack_nframes_t nframes, void *arg) {
	static int quantize_frames = 0;

#define on_quantize_boundary() (!quantize_frames)

	jack_default_audio_sample_t *out_l;
	jack_default_audio_sample_t *out_r;
	jack_default_audio_sample_t *in_l;
	jack_default_audio_sample_t *in_r;

	jack_nframes_t until_quantize, rate, nframes_left, nframes_offset, i;
	int j, group_count, next_bit;
	uint16_t qfield;

	jack_default_audio_sample_t *buffers[2];

	group_t *g;
	file_t *f;

	group_count = state.group_count;

	rate = jack_get_sample_rate(state.client);

	/* initialize each group's output buffers and zero them */
	for( i = 0; i < group_count; i++ ) {
		g = &state.groups[i];

		g->output_buffer_l = out_l = jack_port_get_buffer(g->outport_l, nframes);
		g->output_buffer_r = out_r = jack_port_get_buffer(g->outport_r, nframes);

		for( j = 0; j < nframes; j++ )
			out_l[j] = out_r[j] = 0;
	}

	for( nframes_offset = 0; nframes > 0; nframes -= nframes_left ) {
		if( on_quantize_boundary() ) {
			for( j = 0; j < group_count; j++ ) {
				g = &state.groups[j];
				f = g->active_loop;

				process_file(f);
			}

			qfield = state.monome->quantize_field >> 1;

			for( j = 0; qfield; qfield >>= next_bit ) {
				next_bit = ffs(qfield);
				j += next_bit;

				f = (file_t *) state.monome->callbacks[j].data;
				process_file(f);
			}
		}

		until_quantize   = (state.snap_delay - quantize_frames);
		nframes_left     = MIN(until_quantize, nframes);
		quantize_frames += nframes_left;

		if( quantize_frames >= state.snap_delay - 1 )
			quantize_frames = 0;

		for( j = 0; j < group_count; j++ ) {
			g = &state.groups[j];

			if( !(f = g->active_loop) )
				continue;

			if( !file_is_active(f) )
				continue;

			/* will eventually become an array of arbitrary size for better multichannel support */
			buffers[0] = g->output_buffer_l + nframes_offset;
			buffers[1] = g->output_buffer_r + nframes_offset;

			if( f->process_cb )
				f->process_cb(f, buffers, 2, nframes_left, rate);
		}

		nframes_offset += nframes_left;
	}

	out_l = jack_port_get_buffer(outport_l, nframes_offset);
	out_r = jack_port_get_buffer(outport_r, nframes_offset);
	in_l = jack_port_get_buffer(group_mix_inport_l, nframes_offset);
	in_r = jack_port_get_buffer(group_mix_inport_r, nframes_offset);

	memcpy(out_l, in_l, sizeof(jack_default_audio_sample_t) * nframes_offset);
	memcpy(out_r, in_r, sizeof(jack_default_audio_sample_t) * nframes_offset);

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

void transport_start() {
	jack_transport_start(state.client);
}

void transport_stop() {
	jack_transport_stop(state.client);
	jack_transport_locate(state.client, 0);
}

void r_jack_deactivate() {
	jack_deactivate(state.client);
}

int r_jack_activate() {
	jack_client_t *client = state.client;
	int i, group_count;
	group_t *g;

	if( jack_activate(client) ) {
		fprintf(stderr, "client could not be activated\n");
		return -1;
	}

	connect_to_outports(client);

	group_count = state.group_count;
	for( i = 0; i < group_count; i++ ) {
		g = &state.groups[i];

		jack_connect(client, jack_port_name(g->outport_l), jack_port_name(group_mix_inport_l));
		jack_connect(client, jack_port_name(g->outport_r), jack_port_name(group_mix_inport_r));
	}

	return 0;
}

int r_jack_init() {
	const char *client_name = "rove";
	const char *server_name = NULL;
	jack_options_t options  = JackNoStartServer;
	jack_status_t status;

	int i, group_count, len;
	group_t *g;
	char *buf;

	state.client = jack_client_open(client_name, options, &status, server_name);
	if( state.client == NULL ) {
		fprintf(stderr, "failed to open a connection to the JACK server\n");
		return -1;
	}

	jack_set_process_callback(state.client, process, NULL);
	jack_on_shutdown(state.client, jack_shutdown, 0);

	outport_l = jack_port_register(state.client, "master_out:l", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	outport_r = jack_port_register(state.client, "master_out:r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	group_mix_inport_l = jack_port_register(state.client, "group_mix_in:l", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	group_mix_inport_r = jack_port_register(state.client, "group_mix_in:r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

	group_count = state.group_count;
	for( i = 0; i < group_count; i++ ) {
		g = &state.groups[i];

		len = asprintf(&buf, "group_%d_out:l", g->idx + 1);
		g->outport_l = jack_port_register(state.client, buf, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

		buf[len - 1]  = 'r';
		g->outport_r = jack_port_register(state.client, buf, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
		free(buf);
	}

	return 0;
}
