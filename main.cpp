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

#include <obs/obs-module.h>
#include <lilv-0/lilv/lilv.h>
#include <lilv/lilv.h>
#include <suil-0/suil/suil.h>
#include <suil/suil.h>
#include <lv2/ui/ui.h>
#include <lv2/atom/atom.h>
#include <iostream>
#include <functional>
#include <stdio.h>
#include <string.h>
#include <QWidget>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS LV2 filters";
}

enum LV2PortType
{
	PORT_AUDIO,
	PORT_CONTROL,
	PORT_ATOM,
};

struct LV2Port
{
	uint32_t index;
	bool is_input;
	bool is_optional;
	float value;
	const LilvPort* lilv_port;
	enum LV2PortType type;

};

class LV2Plugin
{
	public:
		LV2Plugin(void);
		~LV2Plugin();

		void list_all(std::function<void(const char *, const char *)> f);

		void set_uri(const char* uri);
		void set_sample_rate(uint32_t sample_rate);
		void set_channels(size_t channels);

		size_t get_channels(void);

		void update_plugin_instance(void);
		void prepare_ports(void);

		void prepare_ui(void);
		void show_ui(void);
		void hide_ui(void);
		bool is_ui_visible(void);
		void cleanup_ui(void);

		void process_frame(float*);

	protected:
		bool ready = false;
		LilvWorld *world;
		const LilvPlugins *plugins = nullptr;

		const LilvPlugin *plugin = nullptr;
		LilvInstance *plugin_instance = nullptr;

		char *plugin_uri = nullptr;
		uint32_t sample_rate = 0;
		size_t channels = 0;
		bool instance_needs_update = true;

		struct LV2Port *ports = nullptr;
		size_t ports_count = 0;
		float *input_buffer = nullptr;
		float *output_buffer = nullptr;
		size_t input_buffer_size = 0;
		size_t output_buffer_size = 0;

		const LilvUI *ui = nullptr;
		const LilvNode *ui_type = nullptr;
		SuilHost *ui_host = nullptr;
		SuilInstance* ui_instance = nullptr;
		QWidget *ui_widget = nullptr;

		static void suil_write_from_ui(void *controller,
					       uint32_t port_index,
					       uint32_t buffer_size,
					       uint32_t format,
					       const void *buffer);

		static uint32_t suil_port_index(void *controller,
						const char *symbol);
};

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

/* SUIL CALLBACKS */
void LV2Plugin::suil_write_from_ui(void *controller,
				   uint32_t port_index,
				   uint32_t buffer_size,
				   uint32_t format,
				   const void *buffer)
{
	LV2Plugin *lv2 = (LV2Plugin*)controller;

	UNUSED_PARAMETER(lv2);

	if (format != 0 || buffer_size != sizeof(float)) {
		printf("gui is trying use format %u with buffer_size %u\n", format, buffer_size);
		return; /* we MUST gracefully ignore according to the spec */
	}

	/* 0 == float */

	lv2->ports[port_index].value = *((float*)buffer);
}


uint32_t LV2Plugin::suil_port_index(void *controller, const char *symbol)
{
	LV2Plugin *lv2 = (LV2Plugin*)controller;

	LilvNode* lilv_sym   = lilv_new_string(lv2->world, symbol);
	const LilvPort* port = lilv_plugin_get_port_by_symbol(lv2->plugin, lilv_sym);

	lilv_node_free(lilv_sym);

	if (port == nullptr) {
		printf("unknown port %s\n", symbol);
		abort();
	}

	auto idx = lilv_port_get_index(lv2->plugin, port);

	/* TODO: untested, keep the printf until you see in in the logs */
	printf("GUI IS GETTING ID FOR PORT %s, got %u\n", symbol, idx);

	return idx;
}


/* THE MEAT */
LV2Plugin::LV2Plugin(void)
{
	world = lilv_world_new();
	lilv_world_load_all(world);
	plugins = lilv_world_get_all_plugins(world);
	plugin = nullptr;
	ui_host = suil_host_new(LV2Plugin::suil_write_from_ui,
				LV2Plugin::suil_port_index,
				NULL, NULL);

	/* XXX: what is ui_hosts's touch callback for exactely? do we even need it? */
}

LV2Plugin::~LV2Plugin()
{
	cleanup_ui();
	suil_host_free(ui_host);
	lilv_world_free(world);
	free(plugin_uri);
}

void LV2Plugin::prepare_ui()
{
	if (this->ui_instance != nullptr)
		return;

	char* bundle_path = lilv_file_uri_parse(lilv_node_as_uri(lilv_ui_get_bundle_uri(this->ui)), NULL);
	char* binary_path = lilv_file_uri_parse(lilv_node_as_uri(lilv_ui_get_binary_uri(this->ui)), NULL);

	/* TODO: last null = features, some pluings seem to need URID */
	this->ui_instance = suil_instance_new(this->ui_host,
					      this,
					      LV2_UI__Qt5UI,
					      this->plugin_uri,
					      lilv_node_as_uri(lilv_ui_get_uri(this->ui)),
					      lilv_node_as_uri(this->ui_type),
					      bundle_path,
					      binary_path,
					      NULL);

	if (this->ui_instance != nullptr) {
		this->ui_widget = (QWidget*) suil_instance_get_widget(ui_instance);
		if (this->ui_widget == nullptr) {
			printf("filed to create widget!\n");
			abort();
		}
		/* TODO: set some hints / properties so its a floting window in my tiling WM */
	} else {
		/* TODO: filtering should help with this */
		printf("filed to find ui!\n");
		abort();
	}
}

void LV2Plugin::show_ui()
{
	if (this->ui_widget != nullptr)
		this->ui_widget->show();
}

void LV2Plugin::hide_ui()
{
	if (this->ui_widget != nullptr)
		this->ui_widget->hide();
}

bool LV2Plugin::is_ui_visible()
{
	if (this->ui_widget == nullptr)
		return false;

	return this->ui_widget->isVisible();
}

void LV2Plugin::cleanup_ui()
{

	if (this->is_ui_visible())
		this->hide_ui();

	this->ui_widget = nullptr;

	if (this->ui_instance != nullptr) {
		suil_instance_free(this->ui_instance);
		this->ui_instance = nullptr;
	}

}

void LV2Plugin::list_all(std::function<void(const char *, const char *)> f)
{
	LILV_FOREACH(plugins, i, this->plugins) {
		bool skip = false;

		auto plugin = lilv_plugins_get(this->plugins, i);

		auto req_features = lilv_plugin_get_required_features(plugin);

		/* XXX: no extra features supported as of now */
		/* TODO: Add support for URID, most of the LSP plugins require it*/
		if (lilv_nodes_size(req_features) > 0) {
			skip = true;
			printf("Filtered out due to unsupported feature %s\n",
			       lilv_node_as_string(lilv_plugin_get_name(plugin)));
		}

		lilv_nodes_free(req_features);

		if (skip)
			continue;


		/* TODO: filter out non filtering (e.g. MIDI) plugins or the ones without GUI */

		f(lilv_node_as_string(lilv_plugin_get_name(plugin)),
		  lilv_node_as_string(lilv_plugin_get_uri(plugin)));
	}
}

void LV2Plugin::set_uri(const char* uri)
{
	bool replace = true;

	if (this->plugin_uri != nullptr && uri != nullptr) {
		if (!strcmp(this->plugin_uri, uri))
			replace = false;
	}

	if (replace) {
		if (this->plugin_uri != nullptr)
			free(this->plugin_uri);


		if (uri != nullptr)
			this->plugin_uri = strdup(uri);
		else
			this->plugin_uri = nullptr;

		instance_needs_update = true;
	}

}

void LV2Plugin::set_sample_rate(uint32_t sample_rate)
{
	if (this->sample_rate != sample_rate) {
		this->sample_rate = sample_rate;
		this->instance_needs_update = true;
	}
}

void LV2Plugin::set_channels(size_t channels)
{
	if (this->channels != channels) {
		this->channels = channels;
		this->instance_needs_update = true;
	}
}

size_t LV2Plugin::get_channels(void)
{
	return this->channels;
}

void LV2Plugin::update_plugin_instance(void)
{
	if (!this->instance_needs_update)
		return;

	this->ready = false;
	this->instance_needs_update = false;

	cleanup_ui();

	lilv_instance_free(this->plugin_instance);
	this->plugin_instance = nullptr;
	this->ui = nullptr;

	auto plugin_uri = lilv_new_uri(this->world, this->plugin_uri);

	if (plugin_uri != nullptr) {
		this->plugin = lilv_plugins_get_by_uri(plugins, plugin_uri);
		lilv_node_free(plugin_uri);

		auto qt5_uri = lilv_new_uri(this->world, LV2_UI__Qt5UI);

		auto uis = lilv_plugin_get_uis(this->plugin);
		LILV_FOREACH(uis, i, uis) {
			const LilvNode *ui_type;
			auto ui = lilv_uis_get(uis, i);

			if (lilv_ui_is_supported(ui, suil_ui_supported,
						 qt5_uri,
						 &ui_type)) {
				this->ui = ui;
				this->ui_type = ui_type;
				break;
			}
		}
		lilv_node_free(qt5_uri);
	}

	if (this->plugin != nullptr) {
		this->plugin_instance = lilv_plugin_instantiate(this->plugin,
								this->sample_rate,
								NULL);
	}

	this->prepare_ports();

	this->ready = true;
}

void LV2Plugin::process_frame(float* buf)
{
	/* XXX: may need proper locking */
	if (!this->ready)
		return;

	for (size_t ch = 0; ch < this->channels; ++ch)
		input_buffer[ch] = buf[ch];

	lilv_instance_run(this->plugin_instance, 1);

	for (size_t ch = 0; ch < this->channels; ++ch)
		buf[ch] = output_buffer[ch];


}

/* OBS PLUGIN STUFF */

#define PROP_PLUGIN_LIST "lv2_plugin_list"
#define PROP_TOGGLE_BUTTON "lv2_toggle_gui_button"

static const char *obs_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "LV2";
}

static void obs_filter_destroy(void *data)
{
	LV2Plugin *lv2 = (LV2Plugin*) data;
	delete lv2;
}

static void obs_filter_update(void *data, obs_data_t *s)
{
	auto obs_audio = obs_get_audio();
	LV2Plugin *lv2 = (LV2Plugin*) data;

	const char *uri = obs_data_get_string(s, PROP_PLUGIN_LIST);

	uint32_t sample_rate = audio_output_get_sample_rate(obs_audio);
	size_t channels = audio_output_get_channels(obs_audio);


	lv2->set_uri(uri);
	lv2->set_sample_rate(sample_rate);
	lv2->set_channels(channels);

	lv2->update_plugin_instance();
}

static void *obs_filter_create(obs_data_t *settings, obs_source_t *filter)
{
	LV2Plugin *lv2 = new LV2Plugin();
	return lv2;
}

static struct obs_audio_data *
obs_filter_audio(void *data, struct obs_audio_data *audio)
{
	LV2Plugin *lv2 = (LV2Plugin*) data;
	float **audio_data = (float **)audio->data;

	size_t channels = lv2->get_channels();
	float buf[channels];

	for (size_t frame = 0; frame < audio->frames; ++frame) {
		for (size_t ch = 0; ch < channels; ++ch) {
			if (audio->data[ch])
				buf[ch] = audio_data[ch][frame];
			else
				buf[ch] = 0.0f;
		}

		lv2->process_frame(buf);

		for (size_t ch = 0; ch < channels; ++ch) {
			if (audio->data[ch])
				audio_data[ch][frame] = buf[ch];
		}

	}

	return audio;
}

static bool obs_toggle_gui(obs_properties_t *props, obs_property_t *property, void *data)
{
	LV2Plugin *lv2 = (LV2Plugin*) data;
	lv2->prepare_ui();

	if (lv2->is_ui_visible())
		lv2->hide_ui();
	else
		lv2->show_ui();

	return true;
}

static obs_properties_t *obs_filter_properties(void *data)
{
	LV2Plugin *lv2 = (LV2Plugin*) data;

	obs_properties_t *props = obs_properties_create();

	obs_property_t *list  = obs_properties_add_list(props,
							PROP_PLUGIN_LIST,
							"List of installed LV2 plugins",
							OBS_COMBO_TYPE_LIST,
							OBS_COMBO_FORMAT_STRING);

	obs_properties_add_button(props,
				  PROP_TOGGLE_BUTTON,
				  "Toggle LV2 Plugin's GUI",
				  obs_toggle_gui);

	obs_property_list_add_string(list, "{select a plug-in}", nullptr);

	lv2->list_all([&](const char *name, const char *uri) {
		obs_property_list_add_string(list, name, uri);
	});

	return props;
}

struct obs_source_info obs_lv2_filter = {
	.id             = "lv2_filter",
	.type           = OBS_SOURCE_TYPE_FILTER,
	.output_flags   = OBS_SOURCE_AUDIO,
	.get_name       = obs_filter_name,
	.create         = obs_filter_create,
	.destroy        = obs_filter_destroy,
	.get_properties = obs_filter_properties,
	.update         = obs_filter_update,
	.filter_audio   = obs_filter_audio,
};

bool obs_module_load(void)
{
	obs_register_source(&obs_lv2_filter);
	return true;
}
