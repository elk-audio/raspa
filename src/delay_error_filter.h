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

#ifndef RASPA_DELAY_ERROR_FILTER_H
#define RASPA_DELAY_ERROR_FILTER_H
/**
 * @brief Initialize filter coefficients.
 *        You need to call this before feeding any value into the filter.
 *
 * @param T60_in_periods Time constant to reach 99.999% of target value, expressed in number of periods (buffers).
 * @param sampling_freq The Sampling Freq in Hertz.
 */
void initialize_delay_error_filter(int T60_in_periods, int sampling_freq);

/**
 * @brief Update delay error filter
 *
 * @param slave_delay_in_frames Time error in frames as reported by the audio controller
 *
 * @return Filtered relative time in nanoseconds, i.e. in the format expected by the driver
 */
int delay_error_filter_tick(int slave_delay_in_frames);

#endif //RASPA_DELAY_ERROR_FILTER_H