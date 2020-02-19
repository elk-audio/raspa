/*
 * Copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk
 * RASPA is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * RASPA is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RASPA.
 * If not, see http://www.gnu.org/licenses/ .
 */

/**
 * @brief API for raspa to create and handle unix sockets and to tx/rx msgs through
 *        them
 * @copyright 2017-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef RASPALIB_COM_TASK_H_
#define RASPALIB_COM_TASK_H_

#include <pthread.h>

#include "fifo.h"

struct CommunicationTaskInstance;

typedef struct CommunicationTaskInstance* ComTask;

/**
 * @brief Create a non-rt communication task for passing non-audio data
 * @return a pointer to a newly created communcation task
 */
ComTask communication_task_create();

/**
 * @brief Deallocate a communication task
 * @param instance
 */
void communication_task_free(ComTask instance);

/**
 * @brief Get a queue for sending data to the task
 * @param instance A ComTask instance
 * @return A pointer to a queue that can be used for pushing messages to the task
 */
CFifo input_queue(ComTask instance);

/**
 * @brief Get a queue for reading data from the task
 * @param instance A ComTask instance
 * @return A pointer to a queue that can be used for receiving messages from the task
 */
CFifo output_queue(ComTask instance);

#endif //RASPALIB_COM_TASK_H_