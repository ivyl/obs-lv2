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

GuiUpdateTimer::GuiUpdateTimer(LV2Plugin *lv2)
{
	this->lv2 = lv2;
}

GuiUpdateTimer::~GuiUpdateTimer()
{
	delete timer;
}

void GuiUpdateTimer::tick(void)
{
	lv2->notify_ui_output_control_ports();
}

void GuiUpdateTimer::start(void)
{
	if (this->timer != nullptr)
		return;

	this->timer = new QTimer(this);
	connect(timer,
		&QTimer::timeout,
		this,
		QOverload<>::of(&GuiUpdateTimer::tick));
	timer->start(33); /* ~30 Hz */
}
