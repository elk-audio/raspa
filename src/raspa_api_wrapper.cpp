/*
 * Copyright 2018-2020 Modern Ancient Instruments Networked AB, dba Elk
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
 * @brief Wrapper around RaspaInterface to map it to the public C API defined in
 *        raspa.h
 * @copyright 2017-2020 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */
#include "raspa/raspa.h"
#include "raspa_intf.h"

static raspa::RaspaIntf raspa_intf;

const char* raspa_get_error_msg(int code)
{
    return raspa_intf.get_error_msg(code);
}

int raspa_init()
{
    return raspa_intf.init();
}

int raspa_open(int buffer_size,
               RaspaProcessCallback process_callback,
               void* user_data, unsigned int debug_flags)
{
    return raspa_intf.open(buffer_size, process_callback, user_data, debug_flags);
}

float raspa_get_sampling_rate()
{
    return raspa_intf.get_sampling_rate();
}

int raspa_get_num_input_channels()
{
    return raspa_intf.get_num_input_channels();
}

int raspa_get_num_output_channels()
{
    return raspa_intf.get_num_output_channels();
}

int raspa_start_realtime()
{
    return raspa_intf.start_realtime();
}

RaspaMicroSec raspa_get_time()
{
    return raspa_intf.get_time();
}

int64_t raspa_get_samplecount()
{
    return raspa_intf.get_samplecount();
}

RaspaMicroSec raspa_get_output_latency()
{
    return raspa_intf.get_output_latency();
}

int raspa_close()
{
    return raspa_intf.close();
}

uint32_t raspa_get_gate_values()
{
    return raspa_intf.get_gate_values();
}

void raspa_set_gate_values(uint32_t cv_gates_out)
{
    raspa_intf.set_gate_values(cv_gates_out);
}