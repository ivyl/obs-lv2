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

/* QT Window Implementation */
WidgetWindow::WidgetWindow(QWidget *parent) : QWidget(parent)
{
	layout.setMargin(0);
	layout.setSpacing(0);
	setLayout(&layout);

	/* BUG: Some controls (e.g. drag and drop points for LSP
	 * Graphic eq) are missplaced until some interaction with the
	 * UI, may be an issue caused by dialog or suil wrapping, needs
	 * debugging - appears with XWayland, need to verify on native X*/
	setWindowFlags(Qt::Dialog);
}

void WidgetWindow::clearWidget(void)
{
	if (this->currentWidget != nullptr)
		this->layout.removeWidget(this->currentWidget);

	this->currentWidget = nullptr;

}

void WidgetWindow::setWidget(QWidget *widget)
{
	this->clearWidget();

	this->currentWidget = widget;
	layout.addWidget(widget);
	this->resize(widget->size());
}

WidgetWindow::~WidgetWindow() {}

void WidgetWindow::closeEvent(QCloseEvent *event)
{
	event->ignore();
	this->hide();
}

/* SUIL CALLBACKS */
void LV2Plugin::suil_write_from_ui(void *controller,
				   uint32_t port_index,
				   uint32_t buffer_size,
				   uint32_t port_protocol,
				   const void *buffer)
{
	LV2Plugin *lv2 = (LV2Plugin*)controller;

	if (port_protocol != PROTOCOL_FLOAT || buffer_size != sizeof(float)) {
		printf("gui is trying use protocol %u with buffer_size %u\n", port_protocol, buffer_size);
		return; /* we MUST gracefully ignore according to the spec */
	}

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

/* UI HANDLING */
void LV2Plugin::prepare_ui()
{
	if (this->ui_instance != nullptr)
		return;

	char* bundle_path = lilv_file_uri_parse(lilv_node_as_uri(lilv_ui_get_bundle_uri(this->ui)), NULL);
	char* binary_path = lilv_file_uri_parse(lilv_node_as_uri(lilv_ui_get_binary_uri(this->ui)), NULL);

	this->ui_instance = suil_instance_new(this->ui_host,
					      this,
					      LV2_UI__Qt5UI,
					      this->plugin_uri,
					      lilv_node_as_uri(lilv_ui_get_uri(this->ui)),
					      lilv_node_as_uri(this->ui_type),
					      bundle_path,
					      binary_path,
					      this->features);

	if (this->ui_instance == nullptr) {
		/* TODO: filtering should help with this */
		printf("filed to find ui!\n");
		abort();
	}

	if (this->ui_window == nullptr)
		this->ui_window = new WidgetWindow();

	auto widget = (QWidget*) suil_instance_get_widget(ui_instance);
	if (widget == nullptr) {
		printf("filed to create widget!\n");
		abort();
	}

	ui_window->setWidget(widget);

	for (size_t i = 0; i < this->ports_count; ++i) {
		auto port = this->ports + i;

		if (port->type != PORT_CONTROL)
			continue;

		suil_instance_port_event(this->ui_instance,
					 port->index,
					 sizeof(float),
					 PROTOCOL_FLOAT,
					 &port->value);
	}
}

void LV2Plugin::show_ui()
{
	if (this->ui_window != nullptr)
		this->ui_window->show();
}

void LV2Plugin::hide_ui()
{
	if (this->ui_window != nullptr)
		this->ui_window->hide();
}

bool LV2Plugin::is_ui_visible()
{
	if (this->ui_window == nullptr)
		return false;

	return this->ui_window->isVisible();
}

void LV2Plugin::cleanup_ui()
{
	if (this->is_ui_visible())
		this->hide_ui();

	if (this->ui_window != nullptr)
		this->ui_window->clearWidget();

	if (this->ui_instance != nullptr) {
		suil_instance_free(this->ui_instance);
		this->ui_instance = nullptr;
	}
}
