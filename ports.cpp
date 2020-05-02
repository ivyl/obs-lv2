/******************************************************************************
 *   Copyright (C) 2020 by Arkadiusz Hiler

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "obs-lv2.hpp"

/* TODO: cleanup_ports, we are now leaking this when switching plugins */
void LV2Plugin::prepare_ports(void)
{
	LilvNode* input_port   = lilv_new_uri(world, LV2_CORE__InputPort);
	LilvNode* output_port  = lilv_new_uri(world, LV2_CORE__OutputPort);
	LilvNode* audio_port   = lilv_new_uri(world, LV2_CORE__AudioPort);
	LilvNode* control_port = lilv_new_uri(world, LV2_CORE__ControlPort);
	LilvNode* atom_port    = lilv_new_uri(world, LV2_ATOM__AtomPort);
	LilvNode* optional     = lilv_new_uri(world, LV2_CORE__connectionOptional);

	this->ports_count = lilv_plugin_get_num_ports(this->plugin);

	this->ports = (LV2Port*) calloc(this->ports_count, sizeof(*this->ports));

	float* default_values = (float*)calloc(this->ports_count, sizeof(float));
	lilv_plugin_get_port_ranges_float(this->plugin, NULL, NULL, default_values);

	input_buffer_size = 0;
	output_buffer_size = 0;

	for (size_t i = 0; i < this->ports_count; ++i) {
		auto port = lilv_plugin_get_port_by_index(this->plugin, i);

		this->ports[i].is_optional = lilv_port_has_property(this->plugin, port, optional);
		this->ports[i].value = isnan(default_values[i]) ? 0.0f : default_values[i];
		this->ports[i].lilv_port = port;
		this->ports[i].index = i;

		if (lilv_port_is_a(this->plugin, port, input_port)) {
			this->ports[i].is_input = true;
		} else if (lilv_port_is_a(this->plugin, port, output_port)) {
			this->ports[i].is_input = false;
		} else {
			printf("No idea what to do with a port that is neither an input nor output\n");
			abort(); /* XXX: check spec and be less harsh */
		}

		if (lilv_port_is_a(this->plugin, port, control_port)) {
			/* they are always float */
			this->ports[i].type = PORT_CONTROL;
			lilv_instance_connect_port(this->plugin_instance, i, &this->ports[i].value);
		} else if (lilv_port_is_a(this->plugin, port, audio_port)) {
			this->ports[i].type = PORT_AUDIO;

			if (this->ports[i].is_input)
				this->input_buffer_size++;
			else
				this->output_buffer_size++;
		} else if (lilv_port_is_a(this->plugin, port, atom_port)) {
			/* TODO: some plugins seem to have atom port what are those? */
			/* everything seems to be working if we ignore them */
			this->ports[i].type = PORT_ATOM;
		} else if (!this->ports[i].is_optional){
			auto name = lilv_port_get_name(this->plugin, port);
			printf("No idea what to do with a port \"%s\" that is neither an audio nor control and is not optional\n", lilv_node_as_string(name));
			auto classes = lilv_port_get_classes(this->plugin, port);
			LILV_FOREACH(nodes, j, classes) {
				auto cls = lilv_nodes_get(classes, j);
				printf("  class: %s\n", lilv_node_as_string(cls));
			}
			abort(); /* XXX: check spec and be less harsh */
		}
	}

	this->input_buffer = (float*) calloc(input_buffer_size, sizeof(*this->input_buffer));
	this->output_buffer = (float*) calloc(output_buffer_size, sizeof(*this->output_buffer));

	size_t in_off = 0;
	size_t out_off = 0;

	for (size_t i = 0; i < this->ports_count; ++i) {
		if (this->ports[i].type == PORT_AUDIO) {
			if (this->ports[i].is_input)
				lilv_instance_connect_port(this->plugin_instance, i, input_buffer + in_off++);
			else
				lilv_instance_connect_port(this->plugin_instance, i, output_buffer + out_off++);
		}
	}

	/* TODO: make sure that we have enough port for our samples */
	/* TODO: wire GUI so it shows levels */
	/* TODO: control port persistance */

	free(default_values);

	lilv_node_free(optional);
	lilv_node_free(atom_port);
	lilv_node_free(control_port);
	lilv_node_free(audio_port);
	lilv_node_free(output_port);
	lilv_node_free(input_port);
}

void LV2Plugin::process_frame(float* buf)
{
	/* XXX: may need proper locking */
	if (!this->ready || this->plugin_instance == nullptr)
		return;

	for (size_t ch = 0; ch < this->channels; ++ch)
		input_buffer[ch] = buf[ch];

	lilv_instance_run(this->plugin_instance, 1);

	for (size_t ch = 0; ch < this->channels; ++ch)
		buf[ch] = output_buffer[ch];


}
