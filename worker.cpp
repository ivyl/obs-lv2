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

#include "worker.hpp"

LV2Worker::LV2Worker(void)
 : work_queue(4096), response_queue(4096), running(false)
{
}

LV2Worker::~LV2Worker()
{
}

void *LV2Worker::process(void *arg) {
	struct WorkItem item;

	LV2Worker *worker = (LV2Worker *) arg;

	while (!worker->canceled) {
		// worker->sem.acquire();
		// if (worker->canceled) break;

		if (!worker->work_queue.try_dequeue(item))
		{
			// printf("worker woken up without work to do!\n");
			continue;
		}

		worker->plugin_interface->work(worker->plugin_instance,
					       LV2Worker::respond, worker,
					       item.size, item.data);
	}

	return NULL;
}

void LV2Worker::start(LV2_Handle instance, LV2_Worker_Interface *interface)
{
	this->plugin_instance = instance;
	this->plugin_interface = interface;

	this->canceled = false;
	/* TODO: make sure we start */
	pthread_create(&thread, NULL, LV2Worker::process, this);
	this->running = true;
}

void LV2Worker::stop()
{
	if (!this->running) return;

	this->canceled = true;
	pthread_join(thread, NULL);
	this->running = false;
}

void LV2Worker::run(void)
{
	struct WorkItem item;

	if (!this->running) return;

	while (this->response_queue.try_dequeue(item))
		this->plugin_interface->work_response(this->plugin_instance,
						      item.size, item.data);

	if (this->plugin_interface->end_run != nullptr)
		this->plugin_interface->end_run(this->plugin_instance);
}


LV2_Worker_Status LV2Worker::schedule_work(LV2_Worker_Schedule_Handle handle,
					   uint32_t size, const void *data)
{
	bool res;
	struct WorkItem item;
	LV2Worker *worker = (LV2Worker *) handle;

	item.data = data;
	item.size = size;

	res = worker->work_queue.try_enqueue(item);

	if (res)
	{
		// worker->sem.release();
		return LV2_WORKER_SUCCESS;
	} else {
		return LV2_WORKER_ERR_NO_SPACE;
	}
}

LV2_Worker_Status LV2Worker::respond(LV2_Worker_Schedule_Handle handle,
				     uint32_t size, const void *data)
{
	bool res;
	struct WorkItem item;
	LV2Worker *worker = (LV2Worker *) handle;

	item.data = data;
	item.size = size;

	res = worker->response_queue.try_enqueue(item);

	if (res) {
		return LV2_WORKER_SUCCESS;
	} else {
		return LV2_WORKER_ERR_NO_SPACE;
	}
}
