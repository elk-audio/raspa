/*
 * Copyright 2018-2021 Modern Ancient Instruments Networked AB, dba Elk
 * RASPA is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * RASPA is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * RASPA. If not, see http://www.gnu.org/licenses/ .
 */

/**
 * @brief Low pass filter for delay locked loop computation for synchronization
 *        with secondary micro-controllers
 * @copyright 2017-2021 Modern Ancient Instruments Networked AB, dba Elk,
 * Stockholm
 */

#ifndef RASPA_RASPA_DELAY_ERROR_FILTER_H
#define RASPA_RASPA_DELAY_ERROR_FILTER_H

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
     */
    RaspaDelayErrorFilter(int T60_in_periods)
    {
        // digital frequency, i.e. one over tau.
        float omega = logf(1000.0f) / static_cast<float>(T60_in_periods);
        float alpha = sinf(omega);
        float comega = cosf(omega);

        float a0 = 1.0f + alpha;
        _b0 = (0.5f * (1.0f - comega)) / a0;
        _b1 = (1.0f - comega) / a0;
        _b2 = (0.5f * (1.0f - comega)) / a0;
        _a1 = (-2.0f * comega) / a0;
        _a2 = (1.0f - alpha) / a0;

        // Reset filter state
        _z1 = 0.0f;
        _z2 = 0.0f;
    }

    /**
     * @brief Update delay error filter
     * @param error_in_ns Time error in ns as reported by the micro controller
     * @return Filtered relative time in nanoseconds, i.e. in the format
     * expected by the driver
     */
    int32_t delay_error_filter_tick(int error_in_ns)
    {
        // filter tick
        float x = static_cast<float>(error_in_ns);
        float y = _b0 * x + _z1;

        _z1 = _b1 * x - _a1 * y + _z2;
        _z2 = _b2 * x - _a2 * y;

        return static_cast<int32_t>(lrintf(y));
    }

private:
    // Coefficients
    float _b0;
    float _b1;
    float _b2;
    float _a1;
    float _a2;
    float _z1;
    float _z2;
};

}  // namespace raspa

#endif  // RASPA_RASPA_DELAY_ERROR_FILTER_H
