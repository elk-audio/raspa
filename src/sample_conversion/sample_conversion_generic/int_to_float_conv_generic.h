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
 * @brief generic algorithm to convert int to float
 * @copyright 2017-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef INT_TO_FLOAT_CONV_GENERIC_H_
#define INT_TO_FLOAT_CONV_GENERIC_H_

#include "sample_conversion_common.h"

/**
 * @brief Single-sample convertion functions.
 * This function assumes the samples are of left justified format
 * @param x signed 24bit left-justified big endian 32bit integer
 * @return float
 */
static inline float sample_int24lj32_to_float32(int32_t x)
{
    float y = (x >> 8) * INT24_TO_FLOAT_SCALING_FACTOR;
    return y;
}

/**
 * @brief left-justified int to float conversion for variable buffer size.
 * @param dst IEEE 32bit float little-endian, non-interleaved buffer of 512
 *            frames in length
 * @param src signed 24bit left-justified big endian 32bit integers, interleaved
 *            buffer of 512 frames in length
 * @param num_frames The num of frames in the buffer
 */
static inline void int24lj32i_to_float32n_generic_size(float* dst, int32_t* src,
                                                                int num_frames)
{
    int n, k;
    for (n = 0; n < num_frames; n++)
    {
        for (k = 0; k < RASPA_N_TOTAL_CHANNELS; k++)
        {
            int32_t x = *src++;
            float y = sample_int24lj32_to_float32(x);
            dst[(k * num_frames) + n] = y;
        }
    }
}

/**
 * @brief Macro to generate an optimized left-justified int to float conversion
 *       for num_frames buffer size.
 * @param dst IEEE 32bit float little-endian, non-interleaved buffer of num_frames
 *            in length
 * @param src signed 24bit left-justified big endian 32bit integers, interleaved
 *            buffer of num_frames in length
 */
#define RASPA_DECLARE_INT_TO_FLOAT_CONV_OPTIMIZED(num_frames)                     \
static inline void int24lj32i_to_float32n_b##num_frames(float* dst, int32_t* src) \
{                                                                                 \
    int n, k;                                                                     \
    for (n = 0; n < num_frames; n++)                                              \
    {                                                                             \
        for (k = 0; k < RASPA_N_TOTAL_CHANNELS; k++)                              \
        {                                                                         \
            int32_t x = *src++;                                                   \
            float y = sample_int24lj32_to_float32(x);                             \
            dst[(k * num_frames) + n] = y;                                        \
        }                                                                         \
    }                                                                             \
}                                                                                 \

RASPA_DECLARE_INT_TO_FLOAT_CONV_OPTIMIZED(8)
RASPA_DECLARE_INT_TO_FLOAT_CONV_OPTIMIZED(16)
RASPA_DECLARE_INT_TO_FLOAT_CONV_OPTIMIZED(32)
RASPA_DECLARE_INT_TO_FLOAT_CONV_OPTIMIZED(64)
RASPA_DECLARE_INT_TO_FLOAT_CONV_OPTIMIZED(128)
RASPA_DECLARE_INT_TO_FLOAT_CONV_OPTIMIZED(256)
RASPA_DECLARE_INT_TO_FLOAT_CONV_OPTIMIZED(512)

/**
 * @brief left-justified int to float. It will call the right function depending
 *        on the buffer size.
 * @param dst IEEE 32bit float little-endian, non-interleaved buffer of
 *            buffer_size frames in length
 * @param src signed 24bit left-justified big endian 32bit integers, interleaved
 *            buffer of buffer_size frames in length
 * @param buffer_size The number of frames in the audio buffer.
 */
static inline void int24lj32i_to_float32n(float* dst,
                                          int32_t* src,
                                          int buffer_size)
{
    switch (buffer_size)
    {
    case 8:
        int24lj32i_to_float32n_b8(dst, src);
        break;

    case 16:
        int24lj32i_to_float32n_b16(dst, src);
        break;

    case 32:
        int24lj32i_to_float32n_b32(dst, src);
        break;

    case 64:
        int24lj32i_to_float32n_b64(dst, src);
        break;

    case 128:
        int24lj32i_to_float32n_b128(dst, src);
        break;

    case 256:
        int24lj32i_to_float32n_b256(dst, src);
        break;

    case 512:
        int24lj32i_to_float32n_b512(dst, src);
        break;

    default:
        int24lj32i_to_float32n_generic_size(dst, src, buffer_size);
        break;
    }
}

#endif