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
 * @brief Low pass filter for delay locked loop computation for synchronization
 *        with secondary micro-controllers
 * @copyright 2017-2020 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef RASPA_DELAY_ERROR_FILTER_H
#define RASPA_DELAY_ERROR_FILTER_H

#include <cmath>

namespace raspa {

class RaspaDelayErrorFilter
{
public:
    /**
     * @brief Initialize the filter and its coefficients.
     *
     * @param T60_in_periods Time constant to reach 99.999% of target value,
     *        expressed in number of periods (buffers).
     * @param sampling_freq The Sampling Freq in Hertz.
     */
    RaspaDelayErrorFilter(int T60_in_periods, int sampling_freq)
    {
        // digital frequency, i.e. one over tau.
        float omega = logf(1000.0f) / ((float) T60_in_periods);
        float alpha = sinf(omega);
        float comega = cosf(omega);

        float a0 = 1.0f + alpha;
        _b0 = ( 0.5f * (1.0f - comega) ) / a0;
        _b1 = ( 1.0f - comega ) / a0;
        _b2 = ( 0.5f * (1.0f - comega) ) / a0;
        _a1 = ( -2.0f * comega ) / a0;
        _a2 = ( 1.0f - alpha ) / a0;

        // Reset filter state
        _z1 = 0.0f;
        _z2 = 0.0f;

        _sampling_period_nanosec = (1.0e9f / sampling_freq);
    }

    /**
     * @brief Update delay error filter
     * @param delay_in_frames Time error in terms of number of audio frames as
     *        reported by the audio controller
     * @return Filtered relative time in nanoseconds, i.e. in the format expected
     *         by the driver
     */
    int delay_error_filter_tick(int delay_in_frames)
    {
        // filter tick
        float x = (float) delay_in_frames;
        float y = _b0 * x + _z1;

        _z1 = _b1 * x - _a1 * y + _z2;
        _z2 = _b2 * x - _a2 * y;

        return (int) lrintf(y * _sampling_period_nanosec);
    }

private:
    // Coeffficients
    float _b0;
    float _b1;
    float _b2;
    float _a1;
    float _a2;
    float _z1;
    float _z2;

    float _sampling_period_nanosec;
};

}

#endif // RASPA_DELAY_ERROR_FILTER_H_