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
 * @brief API to create a Lock free, single consumer, single producer message
 *        queue. This is for a communication channel between RT and NRT threads,
 *        mainly to handle msgs intended for SENSEI.
 */
#ifndef RASPALIB_FIFO_H
#define RASPALIB_FIFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <stdint.h>
#include <errno.h>

struct CFifoInstance;
typedef struct CFifoInstance* CFifo;

/**
 * @brief Open a CFIFO instance.
 *
 * @param max_elem_size The maximum size in bytes for each element.
 * @param max_elements The max number of elements.
 * @return CFifo Returns an instance of the newly created CFifo
 */
CFifo c_fifo_open(size_t max_elem_size, size_t max_elements);

/**
 * @brief Closes the CFIFO and deletes all allocated memory.
 *
 * @param instance The CFIFO instance to be closed.
 */
void c_fifo_close(CFifo instance);

/**
 * @brief Check if the CFIFO is empty
 *
 * @param instance The CFIFO instance.
 * @return int 1 if true, 0 if false.
 */
int c_fifo_is_empty(CFifo instance);

/**
 * @brief Send a message to be put into the CFIFO.
 *
 * @param instance The CFIFO instance.
 * @param data_buffer The buffer which contains the data to be copied into the
 *                    CFIFO
 * @param len The len of the data in bytes. Should be less than the max_elem_size
 *            of the CFIO, otherwise a SIGABRT is triggered by a dynamic assertion.
 * @return size_t Returns the number of bytes sent to the CFIFO.
 */
size_t c_fifo_send(CFifo instance, const char* data_buffer, size_t len);

/**
 * @brief Receive a message from the CFIFO.
 *
 * @param instance The CFIFO instance.
 * @param data_buffer The buffer to where the data will be copied from the CFIFO
 * @param len The len of the data in bytes. Should be less than the max_elem_size
 *            of the CFIO, otherwise a SIGABRT is triggered by a dynamic assertion.
 * @return size_t Returns the number of bytes rxed to the CFIFO.
 */
size_t c_fifo_receive(CFifo instance, char* data_buffer, size_t len);

#ifdef __cplusplus
}
#endif

#endif //RASPALIB_FIFO_H