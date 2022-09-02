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

#include <pthread.h>
#include <unistd.h>
#include <lv2/worker/worker.h>
#include <QSemaphore>
#include "readerwriterqueue.h"

struct WorkItem
{
	uint32_t size;
	const void *data;
};

class LV2Worker
{
private:
	LV2_Handle plugin_instance;
	LV2_Worker_Interface *plugin_interface;
	moodycamel::ReaderWriterQueue<struct WorkItem> work_queue;
	moodycamel::ReaderWriterQueue<struct WorkItem> response_queue;

	pthread_t thread;
	bool running;
	bool canceled;

	static void *process(void *arg);
	QSemaphore sem;

public:
	LV2Worker(void);
	~LV2Worker();

	void start(LV2_Handle instance, LV2_Worker_Interface *interface);
	void stop();
	void run(void);

	static LV2_Worker_Status schedule_work(LV2_Worker_Schedule_Handle handle,
						uint32_t size, const void *data);

	static LV2_Worker_Status respond(LV2_Worker_Respond_Handle handle,
					 uint32_t size, const void *data);
};
