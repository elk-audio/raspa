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
 * @brief Header file which deals converting the samples from integer to
 * floating point
 * @copyright 2017-2021 Modern Ancient Instruments Networked AB, dba Elk,
 * Stockholm
 */

#ifndef RASPA_SAMPLE_CONVERSION_H
#define RASPA_SAMPLE_CONVERSION_H

#include <memory>
#include <utility>
#include <cstring>

#include "driver_config.h"

namespace raspa {

/**
 * scaling factors for 24 bit samples
 */
constexpr float FLOAT_TO_INT24_SCALING_FACTOR = 8388607.0f;  // 2**23 - 1
constexpr float INT24_TO_FLOAT_SCALING_FACTOR =
                    1.19209304e-07f;  // 1.0 / (2**23 - 1)

/**
 * scaling factors for 32 bit samples
 */
constexpr float FLOAT_TO_INT32_SCALING_FACTOR = 2147483647.0f;  // 2**31 - 1
constexpr float INT32_TO_FLOAT_SCALING_FACTOR =
                    4.656612875e-10f;  // 1.0 / (2**31 - 1)

constexpr auto DEFAULT_CODEC_FORMAT = driver_conf::CodecFormat::INT24_LJ;
constexpr int SUPPORTED_BUFFER_SIZES[] = {8, 16, 32, 48, 64, 128, 192, 256, 512};


// Macro to iterate through all possible buffer size and stride combinations and return the right instantiation of
// the sample converter. CONVERTER_SUPPORTED_BUFFER_SIZE must reflect buffer sizes supported.
#define GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, stride)     \
switch (buffer_size)                                                                                     \
{                                                                                                        \
    case 8:                                                                                              \
        return std::make_unique<SampleConverter<8, format, stride>>(sw_chan_id, hw_chan_start_index);    \
        break;                                                                                           \
    case 16:                                                                                             \
        return std::make_unique<SampleConverter<16, format, stride>>(sw_chan_id, hw_chan_start_index);   \
        break;                                                                                           \
    case 32:                                                                                             \
        return std::make_unique<SampleConverter<32, format, stride>>(sw_chan_id, hw_chan_start_index);   \
        break;                                                                                           \
    case 48:                                                                                             \
        return std::make_unique<SampleConverter<48, format, stride>>(sw_chan_id, hw_chan_start_index);   \
        break;                                                                                           \
    case 64:                                                                                             \
        return std::make_unique<SampleConverter<64, format, stride>>(sw_chan_id, hw_chan_start_index);   \
        break;                                                                                           \
    case 128:                                                                                            \
        return std::make_unique<SampleConverter<128, format, stride>>(sw_chan_id, hw_chan_start_index);  \
        break;                                                                                           \
    case 192:                                                                                            \
        return std::make_unique<SampleConverter<192, format, stride>>(sw_chan_id, hw_chan_start_index);  \
        break;                                                                                           \
    case 256:                                                                                            \
        return std::make_unique<SampleConverter<256, format, stride>>(sw_chan_id, hw_chan_start_index);  \
        break;                                                                                           \
    case 512:                                                                                            \
        return std::make_unique<SampleConverter<512, format, stride>>(sw_chan_id, hw_chan_start_index);  \
        break;                                                                                           \
                                                                                                         \
default:                                                                                                 \
    return std::unique_ptr<BaseSampleConverter>(nullptr);                                                \
    break;                                                                                               \
}                                                                                                        \

#define GET_CONVERTER_WITH_STRIDES(sw_chan_id, buffer_size, format, hw_chan_start_index, stride)         \
switch(stride)                                                                                           \
{                                                                                                        \
    case 2:                                                                                              \
        GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, 2);         \
        break;                                                                                           \
    case 4:                                                                                              \
        GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, 4);         \
        break;                                                                                           \
    case 6:                                                                                              \
        GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, 6);         \
        break;                                                                                           \
    case 8:                                                                                              \
        GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, 8);         \
        break;                                                                                           \
    case 10:                                                                                             \
        GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, 10);        \
        break;                                                                                           \
    case 12:                                                                                             \
        GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, 12);        \
        break;                                                                                           \
    case 14:                                                                                             \
        GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, 14);        \
        break;                                                                                           \
    case 16:                                                                                             \
        GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, 16);        \
        break;                                                                                           \
    case 24:                                                                                             \
        GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, 24);        \
        break;                                                                                           \
    case 32:                                                                                             \
        GET_CONVERTER_WITH_BUFFER_SIZE(sw_chan_id, buffer_size, format, hw_chan_start_index, 32);        \
        break;                                                                                           \
default:                                                                                                 \
    return std::unique_ptr<BaseSampleConverter>(nullptr);                                                \
    break;                                                                                               \
}                                                                                                        \

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
    virtual void codec_format_to_float32n(float* dst, const int32_t* src) = 0;

    /**
     * @brief Interleaves samples and converts it from float32 to the codec's
     *        native format.
     * @param dst The destination buffer which holds the samples in native codec
     *        format
     * @param src The source buffer which holds samples in float32 format
     */
    virtual void float32n_to_codec_format(int32_t* dst, const float* src) = 0;
};

/**
 * @brief Templated class which performs optimized sample conversion from codec
 *        format to float and vice versa. It only operates on a single sw channel
 *        and hw channel and can convert them back and forth
 * @tparam buffer_size_in_frames The buffer size in frames.
 * @tparam codec_format The codec format.
 * @tparam buffer_size_in_frames The buffer size in frames
 */
template<int buffer_size_in_frames,
         driver_conf::CodecFormat codec_format,
         int chan_stride>
class SampleConverter : public BaseSampleConverter
{
public:
    /**
     * @brief Construct a new Input Sample Converter object.
     *
     * @param chan_stride THe number of words between each sample of a channel
     * @param sw_chan_id Represents which sw channel this sample converter is
     *                   responsible for.
     * @param hw_chan_start_index The index of the first sample of the hw channel in the
     *                    integer buffer
     */
    SampleConverter(int sw_chan_id, int hw_chan_start_index) :
                                    _hw_chan_start_index(hw_chan_start_index)
    {
        _sw_chan_start_index = sw_chan_id * buffer_size_in_frames;
    }

    ~SampleConverter() = default;

    /**
     * @brief deinterleaves samples and converts it from the native codec format
     *        to float32
     * @param dst The destination buffer which holds the float samples of all the channels
     * @param src The source buffer which holds samples in native codec format
     */
    void codec_format_to_float32n(float* dst, const int32_t* src) override
    {
        int hw_chan_index = _hw_chan_start_index;

        for (int n = 0; n < buffer_size_in_frames; n++)
        {
            if constexpr (codec_format == driver_conf::CodecFormat::BINARY)
            {
                // if codec data is raw binary, directly write it into float buffer
                std::memcpy(&dst[_sw_chan_start_index + n], &src[hw_chan_index], sizeof(int32_t));
            }
            else
            {
                auto sample = _codec_format_to_int32(src[hw_chan_index]);
                dst[_sw_chan_start_index + n] = _int32_to_float32n(sample);
            }
            hw_chan_index += chan_stride;
        }
    }

    void float32n_to_codec_format(int32_t* dst, const float* src) override
    {
        auto hw_chan_index = _hw_chan_start_index;
        for (int n = 0; n < buffer_size_in_frames; n++)
        {
            if constexpr (codec_format == driver_conf::CodecFormat::BINARY)
            {
                // if data is raw binary, directly write it into int buffer
                std::memcpy(&dst[_hw_chan_start_index + (n * chan_stride)], &src[_sw_chan_start_index + n], sizeof(int32_t));
            }
            else
            {
                float x = src[_sw_chan_start_index + n];

                if (x < -1.0f)
                {
                    x = -1.0f;
                }
                else if (x > 1.0f)
                {
                    x = 1.0f;
                }

                auto sample = _float32n_to_int32(x);
                dst[hw_chan_index] = _int32_to_codec_format(sample);
                hw_chan_index += chan_stride;

            }
        }
    }

private:
    /**
     * @brief Converts samples in native codec format to int32. If the sample
     *        resolution is 24 bit, then the bits are right justified to form
     *        a 32 bit integer
     * @param sample The sample in native codec format
     * @return The sample in int32 format
     */
    int32_t _codec_format_to_int32(int32_t sample)
    {
        if constexpr (codec_format == driver_conf::CodecFormat::INT24_LJ)
        {
            return sample >> 8;
        }
        else if constexpr (codec_format == driver_conf::CodecFormat::INT24_I2S)
        {
            /**
             * This format does not have the sign info in the first bit.
             * So we need to manually extend the sign bits to convert it to
             * int32_rj. Fastest way is to use two shifts.
             */
            sample = sample << 1;
            return sample >> 8;
        }
        else if constexpr (codec_format == driver_conf::CodecFormat::INT24_RJ)
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
            /**
             * When codec format is either INT24_32/INT32/binary, the samples are
             * already 32 bits.
             */
            return sample;
        }
    }

    /**
     * @brief Converts an integer sample to float by taking into account the
     *        codec data resolution in bits and normalizing the integer sample
     *        to that range.
     *
     * @param sample The integer sample in the range (2^(codec_res - 1) - 1) to
     *               (-2^(codec_res - 1))
     * @return float Sample represented between -1.0 to 1.0
     */
    float _int32_to_float32n(int32_t sample)
    {
        if constexpr (codec_format == driver_conf::CodecFormat::INT32)
        {
            return sample * INT32_TO_FLOAT_SCALING_FACTOR;
        }
        else
        {
            return sample * INT24_TO_FLOAT_SCALING_FACTOR;
        }
    }

    /**
     * @brief Converts sample in int32 format to native codec format. If the
     *        sample resolution is 24 bits, then the data is left justified.
     * @param sample The sample in int32 format
     * @return The sample in native codec format
     */
    int32_t _int32_to_codec_format(int32_t sample)
    {
        if constexpr (codec_format == driver_conf::CodecFormat::INT24_LJ)
        {
            return sample << 8;
        }
        else if constexpr (codec_format == driver_conf::CodecFormat::INT24_I2S)
        {
            return (sample << 7) & 0x7FFFFF80;
        }
        else if constexpr (codec_format == driver_conf::CodecFormat::INT24_RJ)
        {
            return sample & 0x00FFFFFF;
        }
        else
        {
            /**
             * When codec format is either INT24_32 or INT32, the samples are
             * already the same as the codec format.
             */
            return sample;
        }
    }

    /**
     * @brief Converts a float sample to integer by taking into account the
     *        codec data resolution in bits.
     *
     * @param sample Sample represented between -1.0 to 1.0
     * @return int32_t The integer sample in the range (2^(codec_res - 1) - 1)
     *                 to (-2^(codec_res - 1))
     */
    int32_t _float32n_to_int32(float sample)
    {
        if constexpr (codec_format == driver_conf::CodecFormat::INT32)
        {
            return static_cast<int32_t>(sample * FLOAT_TO_INT32_SCALING_FACTOR);
        }
        else
        {
            return static_cast<int32_t>(sample * FLOAT_TO_INT24_SCALING_FACTOR);
        }
    }

    int _hw_chan_start_index;
    int _sw_chan_start_index;
};

/**
 * @brief Iterate through all possible combinations of buffer size, codec format
 *        and channel strides and return the right instantiation of the sample
 *        converter
 *
 * @param codec_format The codec format
 * @param buffer_size_in_frames The buffer size in frames
 * @param chan_stride The stride between samples of the same channel in the buffer
 *                    i.e the spacing between samples
 * @param sw_chan_id The sw chan id
 *  - after int to float conversion, the resulting sample will be put in the float_buffer[sw_chan_id].
 *   -for float t0 int conversion, samples are taken from float_buffer[sw_chan_id]
 * @param hw_chan_start_index The index in the integer buffer where the first sample of the channel is
 * @return std::unique_ptr<BaseSampleConverter> Instance to SampleConverter
 */
std::unique_ptr<BaseSampleConverter> get_sample_converter(int sw_chan_id,
                                                          int buffer_size_in_frames,
                                                          driver_conf::CodecFormat codec_format,
                                                          int hw_chan_start_index,
                                                          int chan_stride)
{
    switch (codec_format)
    {
    case driver_conf::CodecFormat::INT24_LJ:
        GET_CONVERTER_WITH_STRIDES(sw_chan_id, buffer_size_in_frames, driver_conf::CodecFormat::INT24_LJ, hw_chan_start_index, chan_stride);
        break;

    case driver_conf::CodecFormat::INT24_I2S:
        GET_CONVERTER_WITH_STRIDES(sw_chan_id, buffer_size_in_frames, driver_conf::CodecFormat::INT24_I2S, hw_chan_start_index, chan_stride);
        break;

    case driver_conf::CodecFormat::INT24_RJ:
        GET_CONVERTER_WITH_STRIDES(sw_chan_id, buffer_size_in_frames, driver_conf::CodecFormat::INT24_RJ, hw_chan_start_index, chan_stride);
        break;

    case driver_conf::CodecFormat::INT24_32RJ:
        GET_CONVERTER_WITH_STRIDES(sw_chan_id, buffer_size_in_frames, driver_conf::CodecFormat::INT24_32RJ, hw_chan_start_index, chan_stride);
        break;

    case driver_conf::CodecFormat::INT32:
        GET_CONVERTER_WITH_STRIDES(sw_chan_id, buffer_size_in_frames, driver_conf::CodecFormat::INT32, hw_chan_start_index, chan_stride);
        break;

    case driver_conf::CodecFormat::BINARY:
        GET_CONVERTER_WITH_STRIDES(sw_chan_id, buffer_size_in_frames, driver_conf::CodecFormat::BINARY, hw_chan_start_index, chan_stride);
        break;

    default:
        return std::unique_ptr<BaseSampleConverter>(nullptr);
        break;
    }
}

}  // namespace raspa

#endif  // RASPA_SAMPLE_CONVERSION_H
