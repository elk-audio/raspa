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
 * @brief Lowpass filter for DLL computation in asynchronous driver. Used in cases
 *        where the driver triggers an auxillary microcontroller to rx/tx audio
 *        samples. This filter makes sure that the periodicity of trigger is
 *        accurate.
 * @copyright 2017-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#include <math.h>

#include "raspa/raspa.h"
#include "delay_error_filter.h"

static struct
{
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float z1;
    float z2;
    float sampling_period_nanosec;
} _filter;

void initialize_delay_error_filter(int T60_in_periods, int sampling_freq)
{
    float omega, alpha, a0, comega;

    // digital frequency, i.e. one over tau.
    omega = logf(1000.0f) / ((float) T60_in_periods);
    alpha = sinf(omega);
    comega = cosf(omega);

    a0 = 1.0f + alpha;
    _filter.b0 = ( 0.5f * (1.0f - comega) ) / a0;
    _filter.b1 = ( 1.0f - comega ) / a0;
    _filter.b2 = ( 0.5f * (1.0f - comega) ) / a0;
    _filter.a1 = ( -2.0f * comega ) / a0;
    _filter.a2 = ( 1.0f - alpha ) / a0;
    // Reset filter state
    _filter.z1 = 0.0f;
    _filter.z2 = 0.0f;

    _filter.sampling_period_nanosec = (1.0e9f / sampling_freq);
}

int delay_error_filter_tick(int slave_delay_in_frames)
{
    float x, y;
    int delay_ns;

    // filter tick
    x = (float) slave_delay_in_frames;
    y = _filter.b0 * x + _filter.z1;
    _filter.z1 = _filter.b1 * x - _filter.a1 * y + _filter.z2;
    _filter.z2 = _filter.b2 * x - _filter.a2 * y;

    delay_ns = (int) lrintf(y * _filter.sampling_period_nanosec);
    return delay_ns;
}