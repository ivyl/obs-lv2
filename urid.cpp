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

LV2_URID LV2Plugin::urid_map(LV2_URID_Map_Handle handle, const char *uri)
{
	LV2Plugin *lv2 = (LV2Plugin*)handle;
	LV2_URID urid;

	std::string key = uri;

	if (lv2->urid_map_data.find(key) == lv2->urid_map_data.end()) {
		urid = lv2->current_urid++;
		lv2->urid_map_data[key] = urid;
	} else {
		urid = lv2->urid_map_data[key];
	}

	return urid;
}

const char *LV2Plugin::urid_unmap(void *handle, LV2_URID urid)
{
	LV2Plugin *lv2 = (LV2Plugin*)handle;
	const char *value = nullptr;


	for (auto const& p : lv2->urid_map_data) {
		if (p.second == urid) {
			value = p.first.c_str();
		}
	}

	return value;
}
