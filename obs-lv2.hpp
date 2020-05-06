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
	/* TODO: Q_OBJECT + QT's moc tool ? */
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
	LV2Plugin(void);
	~LV2Plugin();

	void for_each_supported_plugin(std::function<void(const char *, const char *)> f);

	void set_uri(const char* uri);
	void set_sample_rate(uint32_t sample_rate);
	void set_channels(size_t channels);

	size_t get_channels(void);

	void update_plugin_instance(void);
	void prepare_ports(void);
	void cleanup_ports(void);

	void prepare_ui(void);
	void show_ui(void);
	void hide_ui(void);
	bool is_ui_visible(void);
	void cleanup_ui(void);

	void process_frame(float*);

	const char *get_state(void);
	void set_state(const char *str);

	void notify_ui_output_control_ports(void);

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
	float *input_buffer = nullptr;
	float *output_buffer = nullptr;
	size_t input_buffer_size = 0;
	size_t output_buffer_size = 0;

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

	static LV2_URID urid_map(void *handle, const char *uri);
	static const char *urid_unmap(void *handle, LV2_URID urid);

	const LV2_Feature* features[3];

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
