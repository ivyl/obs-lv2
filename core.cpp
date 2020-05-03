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

using namespace std;

LV2Plugin::LV2Plugin(void)
{
	feature_uri_map_data = { this, LV2Plugin::urid_map };
	feature_uri_map = { LV2_URID_MAP_URI, &feature_uri_map_data };

	features[0] = &feature_uri_map;
	features[1] = nullptr; /* NULL terminated */

	world = lilv_world_new();
	lilv_world_load_all(world);
	plugins = lilv_world_get_all_plugins(world);
	plugin = nullptr;
	ui_host = suil_host_new(LV2Plugin::suil_write_from_ui,
				LV2Plugin::suil_port_index,
				NULL, NULL);

	populate_supported_plugins();

	/* XXX: what is ui_hosts's touch callback for exactely? do we even need it? */
}

LV2Plugin::~LV2Plugin()
{
	cleanup_ui();
	suil_host_free(ui_host);
	lilv_world_free(world);
	free(plugin_uri);
}

bool LV2Plugin::is_feature_supported(const LilvNode* node)
{
	bool is_supported = false;

	if (!lilv_node_is_uri(node)) {
		printf("tested feature passed is not an URI!\n");
		abort();
	}

	auto node_uri = lilv_node_as_uri(node);

	for (auto feature = this->features; *feature != nullptr; feature++) {
		if (0 == strcmp((*feature)->URI, node_uri))
			is_supported = true;
	}

	return is_supported;
}

void LV2Plugin::populate_supported_plugins(void)
{
	auto hard_rtc = lilv_new_uri(this->world, LV2_CORE__hardRTCapable);

	LILV_FOREACH(plugins, i, this->plugins) {
		auto plugin = lilv_plugins_get(this->plugins, i);
		bool skip;

		/* require hard RTC */
		/* XXX: seems to be only in the optional features */
		auto opt_features = lilv_plugin_get_optional_features(plugin);
		skip = !lilv_nodes_contains(opt_features, hard_rtc);
		lilv_nodes_free(opt_features);

		if (skip) {
			printf("%s filtered out due to not supporting hard RTC\n",
			       lilv_node_as_string(lilv_plugin_get_name(plugin)));
			continue;
		}

		auto req_features = lilv_plugin_get_required_features(plugin);
		LILV_FOREACH(nodes, j, req_features) {
			const LilvNode* feature = lilv_nodes_get(req_features, j);

			if (!this->is_feature_supported(feature)) {
				skip = true;
				printf("%s filtered out because we do not support %s\n",
				       lilv_node_as_string(lilv_plugin_get_name(plugin)),
				       lilv_node_as_string(feature));
				break;
			}
		}
		lilv_nodes_free(req_features);

		/* TODO: filter out non filtering (e.g. MIDI) plugins or the ones without GUI */

		if (skip)
			continue;

		this->supported_pluggins.push_back(pair<string,string>(
			lilv_node_as_string(lilv_plugin_get_name(plugin)),
			lilv_node_as_string(lilv_plugin_get_uri(plugin))));
	}

	lilv_node_free(hard_rtc);
}

void LV2Plugin::for_each_supported_plugin(function<void(const char *, const char *)> f)
{
	for (auto const& p: this->supported_pluggins)
		f(p.first.c_str(), p.second.c_str());
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

	LilvNode *uri = nullptr;

	if (this->plugin_uri != nullptr) {
		uri = lilv_new_uri(this->world, this->plugin_uri);
		this->plugin = lilv_plugins_get_by_uri(plugins, uri);
		lilv_node_free(uri);
	}

	if (this->plugin != nullptr) {
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

		this->plugin_instance = lilv_plugin_instantiate(this->plugin,
								this->sample_rate,
								this->features);

		this->prepare_ports();
	}

	this->ready = true;
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