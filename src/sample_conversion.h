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
 * @brief Header file which deals converting the samples from integer to floating point
 * @copyright 2017-2029 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef SAMPLE_CONVERSION_H_
#define SAMPLE_CONVERSION_H_

#include <memory>

#include "driver_config.h"

namespace {
constexpr float FLOAT_TO_INT24_SCALING_FACTOR = 8388607.0f;      // 2**23 - 1
constexpr float INT24_TO_FLOAT_SCALING_FACTOR = 1.19209304e-07f; // 1.0 / (2**23 - 1)
}

namespace raspa {

constexpr auto DEFAULT_CODEC_FORMAT = RaspaCodecFormat::INT24_LJ;
constexpr int MIN_NUM_CHANNELS = 2;
constexpr int MAX_NUM_CHANNELS = 8;
constexpr int MIN_BUFFER_SIZE = 8;
constexpr int MAX_BUFFER_SIZE = 1024;

/**
 * @brief Interface class for sample conversion
 */
class BaseSampleConverter
{
public:
    BaseSampleConverter() = default;

    ~BaseSampleConverter() = default;

    /**
     * @brief deinterleaves samples and converts it from the native codec format
     *        to float32
     * @param dst The destination buffer which holds the float samples
     * @param src The source buffer which holds samples in native codec format
     */
    virtual void codec_format_to_float32n(float* dst, int32_t* src) = 0;

    /**
     * @brief Interleaves samples and converts it from float32 to the codec's
     *        native format.
     * @param dst The destination buffer which holds the samples in native codec
     *        format
     * @param src The source buffer which holds samples in float32 format
     */
    virtual void float32n_to_codec_format(int32_t* dst, float* src) = 0;
};

/**
 * @brief Templated class which performs optimized sample conversion. The
 *        optimization comes from the fact that the template parameters control
 *        the inner and outer loops.
 * @tparam codec_format The codec format.
 * @tparam buffer_size_in_frames The buffer size in frames
 * @tparam num_channels The number of channels
 */
template<RaspaCodecFormat codec_format, int buffer_size_in_frames, int num_channels>
class SampleConverter : public BaseSampleConverter
{
public:
    SampleConverter() = default;

    ~SampleConverter() = default;

    /**
     * @brief deinterleaves samples and converts it from the native codec format
     *        to float32
     * @param dst The destination buffer which holds the float samples
     * @param src The source buffer which holds samples in native codec format
     */
    void codec_format_to_float32n(float* dst, int32_t* src) override
    {
        for (int n = 0; n < buffer_size_in_frames; n++)
        {
            for (int k = 0; k < num_channels; k++)
            {
                int32_t x = *src++;
                float y = _codec_format_to_int32rj(x) *
                          INT24_TO_FLOAT_SCALING_FACTOR;
                dst[(k * buffer_size_in_frames) + n] = y;
            }
        }
    }

    /**
     * @brief Interleaves samples and converts it from float32 to the codec's
     *        native format.
     * @param dst The destination buffer which holds the samples in native codec
     *        format
     * @param src The source buffer which holds samples in float32 format
     */
    void float32n_to_codec_format(int32_t* dst, float* src) override
    {
        for (int k = 0; k < num_channels; k++)
        {
            for (int n = 0; n < buffer_size_in_frames; n++)
            {
                float x = *src++;

                if (x < -1.0f)
                {
                    x = -1.0f;
                }
                else if (x > 1.0f)
                {
                    x = 1.0f;
                }

                auto sample = (int32_t) (x * FLOAT_TO_INT24_SCALING_FACTOR);
                dst[(n * num_channels) + k] = _int32rj_to_codec_format(sample);
            }
        }
    }

private:
    /**
     * @brief Converts samples in native codec format to int32rj
     * @param sample The sample in native codec format
     * @return The sample in int32_rj format
     */
    int32_t _codec_format_to_int32rj(int32_t sample)
    {
        if constexpr (codec_format == RaspaCodecFormat::INT24_LJ)
        {
            return sample >> 8;
        }

        else if constexpr (codec_format == RaspaCodecFormat::INT24_I2S)
        {
            /**
             * This format does not have the sign info in the first bit.
             * So we need to manually extend the sign bits to convert it to
             * int32_rj. Fastest way is to use two shifts.
             */
            sample = sample << 1;
            return sample >> 8;
        }
        else if constexpr (codec_format == RaspaCodecFormat::INT24_RJ)
        {
            /**
             * This format does not have the sign info in the first 8 bits.
             * So we need to manually extend the sign bits to convert it to
             * int32_rj. Fastest way is to use two shifts.
             */

            sample = sample << 8;
            return sample >> 8;
        }
        else
        {
            // samples already in int32_rj format
            return sample;
        }
    }

    /**
     * @brief Converts sample in int32_rj format to native codec format.
     * @param sample The sample in int32_rj format
     * @return The sample in native codec format
     */
    int32_t _int32rj_to_codec_format(int32_t sample)
    {
        if constexpr (codec_format == RaspaCodecFormat::INT24_LJ)
        {
            return sample << 8;
        }
        else if constexpr (codec_format == RaspaCodecFormat::INT24_I2S)
        {
            return (sample << 7) & 0x7FFFFF00;
        }
        else if constexpr (codec_format == RaspaCodecFormat::INT24_RJ)
        {
            return sample & 0x00FFFFFF;
        }
        else
        {
            // sample already in codec format
            return sample;
        }
    }
};

/**
 * @brief Gets the next supported buffer size
 * @param buffer_size the current buffer size
 * @return {true, next buffer size} if current buffer size is not equal to the
 *         maximum allowed buffer size.
 *         {false, current buffer size} otherwise
 */
constexpr std::pair<bool, int> get_next_buffer_size(int buffer_size)
{
    if (buffer_size != MAX_BUFFER_SIZE)
    {
        return {true, buffer_size * 2};
    }

    return {false, buffer_size};
}

/**
 * @brief Gets the next supported number of channels
 * @param num_channels the current number of channels
 * @return {true, next number of channels} if current number of channels is not
 *         equal to the maximum allowed number of channels.
 *         {false, current number of channels} otherwise
 */
constexpr std::pair<bool, int> get_next_num_channels(int num_channels)
{
    if (num_channels != MAX_NUM_CHANNELS)
    {
        return {true, num_channels + 2};
    }

    return {false, num_channels};
}

/**
 * @brief Gets the next supported codec format
 * @param codec_format the current codec format
 * @return {true, next codec format} if current number codec format is not
 *         equal to the last possible RaspaCodecFormat.
 *         {false, current codec format} otherwise
 */
constexpr std::pair<bool, RaspaCodecFormat>
get_next_codec_format(RaspaCodecFormat codec_format)
{
    if (codec_format != RaspaCodecFormat::INT32_RJ)
    {
        return {true,
                static_cast<RaspaCodecFormat>(static_cast<int>(codec_format) +
                                              1)};
    }

    return {false, codec_format};
}

/**
 * @brief Get a pointer to an instance of a BaseSampleConvertor object. This
 *        function deduces the template arguments for SampleConverter and
 *        instantiates it for the  following argument values:
 *            - buffer sizes : 8, 16, 32, 64, 128, 256, 512, 1024
 *            - number of channels :  2, 4, 6, 8
 *            - Codec formats : INT24_LJ, INT24_I2S, INT24_RJ, INT32_RJ
 * @param codec_format The codec format
 * @param buffer_size_in_frames The buffer size in frames
 * @param num_channels The number of channels.
 * @return A SampleConverter instance if buffer size and num channels
 *         is supported, empty unique_ptr otherwise.
 */
template<RaspaCodecFormat expected_format = DEFAULT_CODEC_FORMAT,
         int expected_buffer_size = MIN_BUFFER_SIZE,
         int expected_num_chans = MIN_NUM_CHANNELS>
std::unique_ptr<BaseSampleConverter> get_sample_converter(RaspaCodecFormat codec_format,
                                                          int buffer_size_in_frames,
                                                          int num_channels)
{
    if (codec_format != expected_format)
    {
        constexpr auto next_format = get_next_codec_format(expected_format);
        if constexpr (next_format.first)
        {
            return get_sample_converter<next_format.second>(codec_format,
                    buffer_size_in_frames, num_channels);
        }

        return std::unique_ptr<BaseSampleConverter>(nullptr);
    }

    if (buffer_size_in_frames != expected_buffer_size)
    {
        constexpr auto next_buffer_size = get_next_buffer_size(expected_buffer_size);
        if constexpr (next_buffer_size.first)
        {
            return get_sample_converter<expected_format, next_buffer_size.second>
                    (codec_format, buffer_size_in_frames, num_channels);
        }

        return std::unique_ptr<BaseSampleConverter>(nullptr);
    }

    if (num_channels != expected_num_chans)
    {
        constexpr auto next_num_chans = get_next_num_channels(expected_num_chans);
        if constexpr (next_num_chans.first)
        {
            return get_sample_converter<expected_format,
                                        expected_buffer_size,
                                        next_num_chans.second>(codec_format,
                                                buffer_size_in_frames,
                                                num_channels);
        }

        return std::unique_ptr<BaseSampleConverter>(nullptr);
    }

    return std::make_unique<SampleConverter<static_cast<RaspaCodecFormat>
            (expected_format), expected_buffer_size, expected_num_chans>>();
}

} // namespace raspa

#endif // SAMPLE_CONVERSION_H_