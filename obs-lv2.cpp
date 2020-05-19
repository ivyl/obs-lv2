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
#include "obs-lv2.hpp"

#define PROP_PLUGIN_LIST "lv2_plugin_list"
#define PROP_TOGGLE_BUTTON "lv2_toggle_gui_button"

class PluginData
{
	public:
	GuiUpdateTimer *timer;
	LV2Plugin *lv2;
};

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS LV2 filters";
}

static const char *obs_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "LV2";
}

static void obs_filter_update(void *data, obs_data_t *settings);

static void *obs_filter_create(obs_data_t *settings, obs_source_t *filter)
{
	PluginData *data = new PluginData();

	data->lv2 = new LV2Plugin();
	const char *state = obs_data_get_string(settings, "lv2_plugin_state");

	obs_filter_update(data, settings);
	data->lv2->set_state(state);

	data->timer = new GuiUpdateTimer(data->lv2);
	data->timer->start();

	return data;
}

static void obs_filter_destroy(void *data)
{
	PluginData *d = (PluginData*) data;

	delete d->timer;
	delete d->lv2;
}

static bool obs_toggle_gui(obs_properties_t *props, obs_property_t *property, void *data)
{
	LV2Plugin *lv2 = ((PluginData*) data)->lv2;
	lv2->prepare_ui();

	if (lv2->is_ui_visible())
		lv2->hide_ui();
	else
		lv2->show_ui();

	return true;
}

static obs_properties_t *obs_filter_properties(void *data)
{
	LV2Plugin *lv2 = ((PluginData*) data)->lv2;

	obs_properties_t *props = obs_properties_create();

	obs_property_t *list  = obs_properties_add_list(props,
							PROP_PLUGIN_LIST,
							"Plugin",
							OBS_COMBO_TYPE_LIST,
							OBS_COMBO_FORMAT_STRING);

	obs_properties_add_button(props,
				  PROP_TOGGLE_BUTTON,
				  "Toggle LV2 Plugin's GUI",
				  obs_toggle_gui);

	obs_property_list_add_string(list, "{select a plug-in}", "");

	lv2->for_each_supported_plugin([&](const char *name, const char *uri) {
		obs_property_list_add_string(list, name, uri);
	});

	return props;
}

static void obs_filter_update(void *data, obs_data_t *settings)
{
	auto obs_audio = obs_get_audio();
	LV2Plugin *lv2 = ((PluginData*) data)->lv2;

	const char *uri = obs_data_get_string(settings, PROP_PLUGIN_LIST);

	uint32_t sample_rate = audio_output_get_sample_rate(obs_audio);
	size_t channels = audio_output_get_channels(obs_audio);


	if (strlen(uri) == 0)
		lv2->set_uri(nullptr);
	else
		lv2->set_uri(uri);

	lv2->set_sample_rate(sample_rate);
	lv2->set_channels(channels);

	lv2->update_plugin_instance();
}

static struct obs_audio_data *
obs_filter_audio(void *data, struct obs_audio_data *audio)
{
	LV2Plugin *lv2 = ((PluginData*) data)->lv2;
	float **audio_data = (float **)audio->data;

	lv2->process_frames(audio_data, audio->frames);

	return audio;
}

static void obs_filter_save(void *data, obs_data_t *settings)
{
	LV2Plugin *lv2 = ((PluginData*) data)->lv2;
	auto state = lv2->get_state();
	obs_data_set_string(settings, "lv2_plugin_state", state);
	free(state);
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
	.save           = obs_filter_save,
};

bool obs_module_load(void)
{
	obs_register_source(&obs_lv2_filter);
	return true;
}
