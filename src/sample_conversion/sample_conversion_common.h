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
 * @brief common definitions across all sample conversion algos
 * @copyright 2017-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef SAMPLE_CONVERSION_COMMON_H_
#define SAMPLE_CONVERSION_COMMON_H_

#include "common_defs.h"

#define FLOAT_TO_INT24_SCALING_FACTOR   8388607.0f      // 2**23 - 1
#define INT24_TO_FLOAT_SCALING_FACTOR   1.19209304e-07f // 1.0 / (2**23 - 1)
#define INT24_RJ32_MAX_VALUE            8388607
#define INT24_RJ32_MIN_VALUE            -8388607
#define INT24_LJ32_MAX_VALUE            (INT24_RJ32_MAX_VALUE << 8)
#define INT24_LJ32_MIN_VALUE            (INT24_RJ32_MIN_VALUE << 8)

#endif /* SAMPLE_CONVERSION_COMMON_H_ */