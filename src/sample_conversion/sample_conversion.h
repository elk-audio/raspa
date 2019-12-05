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
 * @brief Header file which deals converting the samples from integer to floating point
 * @copyright 2017-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef SAMPLE_CONVERSION_H_
#define SAMPLE_CONVERSION_H_

#define RASPA_INT_TO_FLOAT(dst, src, buffer_size) int24lj32i_to_float32n(dst, \
                                                               src, buffer_size)

#define RASPA_FLOAT_TO_INT(dst, src, buffer_size) float32n_to_int24lj32i(dst, \
                                                               src, buffer_size)

#include "sample_conversion_generic/int_to_float_conv_generic.h"
#include "sample_conversion_generic/float_to_int_conv_generic.h"

#endif // SAMPLE_CONVERSION_H_