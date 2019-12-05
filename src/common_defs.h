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
 * @brief File containing common definitions for raspa.
 * @copyright 2017-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef RASPA_COMMON_DEFS_H_
#define RASPA_COMMON_DEFS_H_

/**
 * Total num of channels
 */
#if (RASPA_N_INPUT_CHANNELS > RASPA_N_OUTPUT_CHANNELS)
#define RASPA_N_TOTAL_CHANNELS RASPA_N_INPUT_CHANNELS
#else
#define RASPA_N_TOTAL_CHANNELS RASPA_N_OUTPUT_CHANNELS
#endif

/**
 * Min and max optimized buffer size. Other buffer sizes are supported but not
 * optimized
 */
#define RASPA_MIN_N_FRAMES_PER_BUFFER 8
#define RASPA_MAX_N_FRAMES_PER_BUFFER 512

#define RASPA_MAX_N_SAMPLES_PER_BUFFER (RASPA_MAX_N_FRAMES_PER_BUFFER * \
                                        RASPA_N_TOTAL_CHANNELS)

#define RASPALIB__REQUIRED_DRIVER_VERSION_MAJ   0
#define RASPALIB__REQUIRED_DRIVER_VERSION_MIN   2

#endif // RASPA_COMMON_DEFS_H_