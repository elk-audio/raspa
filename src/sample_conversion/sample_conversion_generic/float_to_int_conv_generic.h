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
 * @brief generic algorithm to convert float to int
 * @copyright 2017-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef FLOAT_TO_INT_CONV_GENERIC_H_
#define FLOAT_TO_INT_CONV_GENERIC_H_

#include "sample_conversion_common.h"

/**
 * Single-sample convertion functions.
 * These are handy mostly for tests.
 * This function assumes the samples are of left justified format
 */
static inline int32_t sample_float32_to_int24lj32(float x)
{
    if (x < -1.0f)
    {
        x = -1.0f;
    }
    else if (x > 1.0f)
    {
        x = 1.0f;
    }
    return (int32_t )(x * FLOAT_TO_INT24_SCALING_FACTOR) << 8;
}

/**
 * @brief Macro to declare function for float to left-justified int conversion
 *        for num_frames per buffer
 * @param dst signed 24bit left-justified big endian 32bit integers of num_frames
 *            in length
 * @param src IEEE 32bit float little-endian of num_frames in length
 */
#define RASPA_DECLARE_FLOAT_TO_INT_CONV_OPTIMIZED(num_frames)                     \
static inline void float32n_to_int24lj32i_b##num_frames(int32_t* dst, float* src) \
{                                                                                 \
    int n, k;                                                                     \
    for (k = 0; k < RASPA_N_TOTAL_CHANNELS; k++)                                  \
    {                                                                             \
        for (n = 0; n < num_frames; n++)                                          \
        {                                                                         \
            float x = *src++;                                                     \
            int32_t y = sample_float32_to_int24lj32(x);                           \
            dst[n * RASPA_N_TOTAL_CHANNELS + k] = y;                                    \
        }                                                                         \
    }                                                                             \
}                                                                                 \

// Declare the various float to int conversion functions
RASPA_DECLARE_FLOAT_TO_INT_CONV_OPTIMIZED(8)
RASPA_DECLARE_FLOAT_TO_INT_CONV_OPTIMIZED(16)
RASPA_DECLARE_FLOAT_TO_INT_CONV_OPTIMIZED(32)
RASPA_DECLARE_FLOAT_TO_INT_CONV_OPTIMIZED(64)
RASPA_DECLARE_FLOAT_TO_INT_CONV_OPTIMIZED(128)
RASPA_DECLARE_FLOAT_TO_INT_CONV_OPTIMIZED(256)
RASPA_DECLARE_FLOAT_TO_INT_CONV_OPTIMIZED(512)

/**
 * Float to int conversion for a generic buffer size. This is significantly
 * slower to run
 * @param dst signed 24bit left-justified big endian 32bit integers of num_frames
 *            in length
 * @param src IEEE 32bit float little-endian of num_frames in length
 * @param num_frames The buffer size in frames
 */
static inline void float32n_to_int24lj32i_generic_size(int32_t* dst, float* src,
                                                       int num_frames)
{
    int n, k;
    for (k = 0; k < RASPA_N_TOTAL_CHANNELS; k++)
    {
        for (n = 0; n < num_frames; n++)
        {
            float x = *src++;
            int32_t y = sample_float32_to_int24lj32(x);
            dst[n * RASPA_N_TOTAL_CHANNELS + k] = y;
        }
    }
}

/**
 * @brief float to left-justified int conversion optimized for various buffer
 *        sizes
 * @param dst signed 24bit left-justified big endian 32bit integers of
 *            buffer_size frames in length
 * @param src IEEE 32bit float little-endian of buffer_size frames in length
 * @param buffer_size The number of frames in the audio buffer.
 */
static inline void float32n_to_int24lj32i(int32_t* dst,
                                          float* src,
                                          int buffer_size)
{
    switch (buffer_size)
    {
    case 8:
        float32n_to_int24lj32i_b8(dst, src);
        break;

    case 16:
        float32n_to_int24lj32i_b16(dst, src);
        break;

    case 32:
        float32n_to_int24lj32i_b32(dst, src);
        break;

    case 64:
        float32n_to_int24lj32i_b64(dst, src);
        break;

    case 128:
        float32n_to_int24lj32i_b128(dst, src);
        break;

    case 256:
        float32n_to_int24lj32i_b256(dst, src);
        break;

    case 512:
        float32n_to_int24lj32i_b512(dst, src);
        break;

    default:
        // choose variable buffer size implementation
        float32n_to_int24lj32i_generic_size(dst, src, buffer_size);
        break;
    }
}

#endif