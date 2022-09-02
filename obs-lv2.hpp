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

#include <lilv-0/lilv/lilv.h>
#include <lilv/lilv.h>
#include <suil-0/suil/suil.h>
#include <suil/suil.h>
#include <lv2/ui/ui.h>
#include <lv2/atom/atom.h>
#include <lv2/urid/urid.h>
#include <lv2/state/state.h>
#include <lv2/instance-access/instance-access.h>
#include <lv2/data-access/data-access.h>
#include <lv2/options/options.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/worker/worker.h>
#include <iostream>
#include <functional>
#include <stdio.h>
#include <string.h>
#include <QWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <string>
#include <math.h>
#include <vector>
#include <algorithm>

#include "worker.hpp"

/* XXX: OBS gives us ~400 frames at a time, and internal defines seem to point
 * 1k frames max, this should be safe for now - if stop working we need to
 * either bump it here or change the process_frames to process input in chunks
 */
#define MAX_AUDIO_FRAMES 4096

#define WARN printf

class LV2Plugin;

class GuiUpdateTimer : public QObject
{
public:
	GuiUpdateTimer(LV2Plugin *lv2);
	~GuiUpdateTimer();

	void start(void);

protected:
	void tick(void);
	QTimer *timer = nullptr;
	LV2Plugin *lv2 = nullptr;
};

class WidgetWindow : public QWidget
{
public:
	explicit WidgetWindow(QWidget *parent = nullptr);
	void clearWidget(void);
	void setWidget(QWidget *widget);
	virtual ~WidgetWindow();

protected:
	QVBoxLayout layout;
	QWidget *currentWidget = nullptr;

	void closeEvent(QCloseEvent *event) override;
	void updatePorts(void);
};

#define PROTOCOL_FLOAT 0

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
	float ui_value;
	const LilvPort* lilv_port;
	enum LV2PortType type;
};

class LV2Plugin
{
public:
	LV2Plugin(size_t channels);
	~LV2Plugin();

	void for_each_supported_plugin(std::function<void(const char *, const char *)> f);

	void set_uri(const char* uri);
	void set_sample_rate(uint32_t sample_rate);
	void set_channels(size_t channels);

	size_t get_channels(void);

	void update_plugin_instance(void);
	void cleanup_plugin_instance(void);
	void prepare_ports(void);
	void cleanup_ports(void);

	void prepare_ui(void);
	void show_ui(void);
	void hide_ui(void);
	bool is_ui_visible(void);
	void cleanup_ui(void);

	void process_frames(float**, int frames);

	char *get_state(void);
	void set_state(const char *str);

	void notify_ui_output_control_ports(void);

	uint32_t port_index(const char *symbol);

protected:
	bool ready = false;
	LilvWorld *world;
	std::vector<std::pair<std::string,std::string>> supported_pluggins;
	const LilvPlugins *plugins = nullptr;
	void populate_supported_plugins(void);

	const LilvPlugin *plugin = nullptr;
	LilvInstance *plugin_instance = nullptr;

	char *plugin_uri = nullptr;
	uint32_t sample_rate = 0;
	size_t channels = 0;
	bool instance_needs_update = true;

	/* PORT MAPPING */
	struct LV2Port *ports = nullptr;
	size_t ports_count = 0;
	float **input_buffer = nullptr;
	float **output_buffer = nullptr;
	size_t input_channels_count = 0;
	size_t output_channels_count = 0;

	/* UI */
	const LilvUI *ui = nullptr;
	const LilvNode *ui_type = nullptr;
	SuilHost *ui_host = nullptr;
	SuilInstance* ui_instance = nullptr;
	WidgetWindow *ui_window = nullptr;

	bool is_feature_supported(const LilvNode*);

	static void suil_write_from_ui(void *controller,
				       uint32_t port_index,
				       uint32_t buffer_size,
				       uint32_t format,
				       const void *buffer);

	static uint32_t suil_port_index(void *controller,
					const char *symbol);

	/* URID MAP FEATURE */
	std::map<std::string,LV2_URID> urid_map_data;
	LV2_URID current_urid = 1; /* 0 is reserverd */

	LV2_URID_Map feature_uri_map_data;
	LV2_Feature feature_uri_map;

	LV2_URID_Unmap feature_uri_unmap_data;
	LV2_Feature feature_uri_unmap;

	LV2_Feature feature_instance_access;

	LV2_Extension_Data_Feature feature_data_access_data;
	LV2_Feature feature_data_access;

	int32_t feature_option_max_block_length;
	int32_t feature_option_min_block_length;

	LV2_Feature feature_options;
	LV2_Options_Option feature_options_options[3];
	LV2_Feature feature_bounded_block_lenght;

	LV2Worker worker;
	LV2_Worker_Schedule feature_worker_schedule_data;
	LV2_Feature feature_worker_schedule;

	static LV2_URID urid_map(void *handle, const char *uri);
	static const char *urid_unmap(void *handle, LV2_URID urid);

	const LV2_Feature* features[8];

	/* STATE PERSISTENCE */
	static const void *get_port_value(const char *port_symbol,
					  void *user_data,
					  uint32_t *size,
					  uint32_t *type);

	static void set_port_value(const char *port_symbol,
				   void *user_data,
				   const void *value,
				   uint32_t size,
				   uint32_t type);
};
