/* Copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk
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
 * @brief Lock free fifo to handle msgs between RT and NRT threads.
 *        Re-implemented in C from Kjell Hedstroms C++ implementation, using the
 *        same memory orders for atomic read/writes. The main reason for putting
 *        the implementation in a source file and not keeping everything in a
 *        header file, is for compatibility with C++ and GTest. stdatomic.h 
 *        is not compatible with G++.
 */

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "fifo.h"

typedef struct CFifoInstance
{
    atomic_int _head;
    atomic_int _tail;
    size_t     _capacity;
    size_t     _elem_size;
    char*      _storage;
} CFifoInstance;

CFifo c_fifo_open(size_t max_elem_size, size_t max_elements)
{
    CFifoInstance* instance = malloc(sizeof(CFifoInstance));
    instance->_head = 0;
    instance->_tail = 0;
    instance->_capacity = max_elements + 1;
    instance->_elem_size = max_elem_size;
    instance->_storage = malloc(max_elem_size * instance->_capacity);
    return instance;
}

void c_fifo_close(CFifo instance)
{
    if (instance != NULL)
    {
        free(instance->_storage);
        free(instance);
    }
}

int c_fifo_is_empty(CFifo instance)
{
    return (atomic_load(&instance->_head) == atomic_load(&instance->_tail));
}

size_t c_fifo_send(CFifo instance, const char* data_buffer, size_t len)
{
    assert(len <= instance->_elem_size);
    int current_tail = atomic_load_explicit(&instance->_tail, memory_order_relaxed);
    int next_tail = (current_tail + 1) % (int)instance->_capacity;
    if (next_tail == atomic_load_explicit(&instance->_head, memory_order_acquire))
    {
        return 0; // queue full!
    }
    memcpy(&instance->_storage[current_tail * instance->_elem_size], data_buffer, len);
    atomic_store_explicit(&instance->_tail, next_tail, memory_order_release);
    return len;
}

size_t c_fifo_receive(CFifo instance, char* data_buffer, size_t len)
{
    assert(len <= instance->_elem_size);
    int current_head = atomic_load_explicit(&instance->_head, memory_order_relaxed);
    if (current_head == atomic_load_explicit(&instance->_tail, memory_order_acquire))
    {
        return 0; // queue empty!
    }
    memcpy(data_buffer, &instance->_storage[current_head * instance->_elem_size], len);
    atomic_store_explicit(&instance->_head, (current_head + 1) % instance->_capacity, memory_order_release);
    return len;
}