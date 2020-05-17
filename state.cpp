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

#define STATE_URI "http://hiler.eu/obs-lv2/plugin-state"

const void *LV2Plugin::get_port_value(const char *port_symbol,
				      void *user_data,
				      uint32_t *size,
				      uint32_t *type)
{
	LV2Plugin *lv2 = (LV2Plugin*)user_data;

	auto idx = LV2Plugin::suil_port_index(lv2, port_symbol);
	*size = sizeof(float);
	*type = PROTOCOL_FLOAT;

	return &lv2->ports[idx].value;
}

void LV2Plugin::set_port_value(const char *port_symbol,
			       void *user_data,
			       const void *value,
			       uint32_t size,
			       uint32_t type)
{
	LV2Plugin *lv2 = (LV2Plugin*)user_data;

	auto idx = lv2->port_index(port_symbol);

	if (size != sizeof(float) || type != PROTOCOL_FLOAT) {
		printf("trying to restore state for something weird\n");
		abort();
	}

	lv2->ports[idx].value = *((float*) value);
}

char *LV2Plugin::get_state(void)
{
	if (this->plugin_instance == nullptr)
		return NULL;

	auto state = lilv_state_new_from_instance(this->plugin,
			this->plugin_instance,
			&this->feature_uri_map_data,
			NULL, NULL, NULL, NULL,
			LV2Plugin::get_port_value,
			this,
			LV2_STATE_IS_POD,
			this->features);

	auto str = lilv_state_to_string(this->world,
			&this->feature_uri_map_data,
			&this->feature_uri_unmap_data,
			state,
			STATE_URI,
			NULL);

	lilv_state_free(state);

	return str;
}

void LV2Plugin::set_state(const char *str)
{
	if (str == nullptr || this->plugin_instance == nullptr)
		return;

	auto state = lilv_state_new_from_string(this->world,
			&this->feature_uri_map_data,
			str);

	lilv_state_restore(state,
			   this->plugin_instance,
			   LV2Plugin::set_port_value,
			   this,
			   LV2_STATE_IS_POD,
			   this->features);

	lilv_state_free(state);
}
