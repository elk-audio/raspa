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

namespace
{
constexpr float FLOAT_TO_INT24_SCALING_FACTOR = 8388607.0f;      // 2**23 - 1
constexpr float INT24_TO_FLOAT_SCALING_FACTOR = 1.19209304e-07f; // 1.0 / (2**23 - 1)
}

namespace raspa {

/**
 * @brief Interface class for sample consversion
 */
class SampleConverter
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
class SampleConverterOptimized : public SampleConverter
{
public:
    SampleConverterOptimized() = default;

    ~SampleConverterOptimized() = default;

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
 * @brief Class which performs sample conversion for any generic codec format,
 *        buffer sizes or number of channels. This will not be optimized as the
 *        number of iterations in the inner and outer loops are determined at
 *        run time
 */
class SampleConverterGeneric : public SampleConverter
{
public:
    /**
     * @brief Construct a SampleConverterGeneric object
     * @param codec_format The codec format
     * @param buffer_size_in_frames The buffer size in frames
     * @param num_channels The number of channels
     */
    SampleConverterGeneric(RaspaCodecFormat codec_format,
                           int buffer_size_in_frames,
                           int num_channels) : _codec_format(codec_format),
                                               _buffer_size_in_frames(
                                                       buffer_size_in_frames),
                                               _num_channels(num_channels)
    {}

    ~SampleConverterGeneric() = default;

    /**
     * @brief deinterleaves samples and converts it from the native codec format
     *        to float32
     * @param dst The destination buffer which holds the float samples
     * @param src The source buffer which holds samples in native codec format
     */
    void codec_format_to_float32n(float* dst, int32_t* src) override
    {
        for (int n = 0; n < _buffer_size_in_frames; n++)
        {
            for (int k = 0; k < _num_channels; k++)
            {
                int32_t x = *src++;
                float y = _codec_format_to_int32rj(x) *
                          INT24_TO_FLOAT_SCALING_FACTOR;
                dst[(k * _buffer_size_in_frames) + n] = y;
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
        for (int k = 0; k < _num_channels; k++)
        {
            for (int n = 0; n < _buffer_size_in_frames; n++)
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
                dst[(n * _num_channels) + k] = _int32rj_to_codec_format(sample);
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
        if (_codec_format == RaspaCodecFormat::INT24_LJ)
        {
            return sample >> 8;
        }
        else if (_codec_format == RaspaCodecFormat::INT24_I2S)
        {
            /**
             * This format does not have the sign info in the first bit.
             * So we need to manually extend the sign bits to convert it to
             * int32_rj. Fastest way is to use two shifts.
             */
            sample = sample << 1;
            return sample >> 8;
        }
        else if (_codec_format == RaspaCodecFormat::INT24_RJ)
        {
            // CodecFormat::INT24_RJ
            sample = sample << 8;
            return sample >> 8;
        }
        else
        {
            // sample is already in int32rj
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
        if (_codec_format == RaspaCodecFormat::INT24_LJ)
        {
            return sample << 8;
        }
        else if (_codec_format == RaspaCodecFormat::INT24_I2S)
        {
            return (sample << 7) & 0x7FFFFF00;
        }
        else if (_codec_format == RaspaCodecFormat::INT24_RJ)
        {
            return sample & 0x00FFFFFF;
        }
        else
        {
            // its already in codec format
            return sample;
        }
    }

    RaspaCodecFormat _codec_format;
    int _buffer_size_in_frames;
    int _num_channels;
};

/**
 * @brief Instantiate SampleConverterOptimized object using fixed codec_format
 *        and num channels. Supported buffer sizes are 8 - 1024 and should be
 *        a power of 2.
 * @tparam codec_format The RaspaCodecFormat
 * @tparam num_channels The num of channels
 * @param buffer_size_in_frames The buffer size in frames.
 * @return SampleConverterOptimized object if buffer size is supported,
 * SampleConverterGeneric object otherwise.
 */
template<RaspaCodecFormat codec_format, int num_channels>
std::unique_ptr<SampleConverter> get_sample_converter(int buffer_size_in_frames)
{
    switch (buffer_size_in_frames)
    {
    case 8:
        return std::make_unique<SampleConverterOptimized<codec_format,
                8, num_channels>>();

    case 16:
        return std::make_unique<SampleConverterOptimized<codec_format,
                16, num_channels>>();

    case 32:
        return std::make_unique<SampleConverterOptimized<codec_format,
                32, num_channels>>();

    case 64:
        return std::make_unique<SampleConverterOptimized<codec_format,
                64, num_channels>>();

    case 128:
        return std::make_unique<SampleConverterOptimized<codec_format,
                128, num_channels>>();

    case 256:
        return std::make_unique<SampleConverterOptimized<codec_format,
                256, num_channels>>();

    case 512:
        return std::make_unique<SampleConverterOptimized<codec_format,
                512, num_channels>>();

    case 1024:
        return std::make_unique<SampleConverterOptimized<codec_format,
                1024, num_channels>>();

    default:
        return std::make_unique<SampleConverterGeneric>(codec_format,
                                                        buffer_size_in_frames,
                                                        num_channels);
    }
}

/**
 * @brief Instantiate SampleConverterOptimized object using fixed codec_format
 *        but variable buffer size and num channels. Supported num channels are
 *        2,4,6 and 8.
 * @tparam codec_format The RaspaCodecFormat
 * @param num_channels The num of channels
 * @param buffer_size_in_frames The buffer size in frames.
 * @return SampleConverterOptimized object if buffer size and num channels is
 *         supported, SampleConverterGeneric object otherwise.
 */
template<RaspaCodecFormat codec_format>
std::unique_ptr<SampleConverter> get_sample_converter(int buffer_size_in_frames,
                                                      int num_channels)
{
    switch (num_channels)
    {
    case 2:
        return get_sample_converter<codec_format, 2>(buffer_size_in_frames);
    case 4:
        return get_sample_converter<codec_format, 4>(buffer_size_in_frames);
    case 8:
        return get_sample_converter<codec_format, 8>(buffer_size_in_frames);
    default:
        return std::make_unique<SampleConverterGeneric>(codec_format,
                                                        buffer_size_in_frames,
                                                        num_channels);
    }
}

/**
 * @brief Get a pointer to an instance of a SampleConvertor object. Depending
 *        on the arguments, either a SampleConverterOptimized or
 *        SampleConverterGeneric object is instantiated.
 *
 *        A SampleConverterOptimized object will be instantiated for the
 *        following argument values:
 *        buffer sizes : 8, 16, 32, 64, 128, 256, 512, 1024
 *        number of channels :  2, 4, 6, 8
 *        Codec formats : INT24_LJ, INT24_I2S, INT24_RJ, INT32_RJ
 * @param codec_format The codec format
 * @param buffer_size_in_frames The buffer size in frames
 * @param num_channels The number of channels.
 * @return A SampleConverterOptimized instance if buffer size and num channels
 *         is supported, SampleConverterGeneric instance otherwise.
 */
std::unique_ptr<SampleConverter>
get_sample_converter(RaspaCodecFormat codec_format,
                     int buffer_size_in_frames,
                     int num_channels)
{
    switch (codec_format)
    {
    case RaspaCodecFormat::INT24_LJ:
        return get_sample_converter<RaspaCodecFormat::INT24_LJ>
                (buffer_size_in_frames, num_channels);

    case RaspaCodecFormat::INT24_I2S:
        return get_sample_converter<RaspaCodecFormat::INT24_I2S>
                (buffer_size_in_frames, num_channels);

    case RaspaCodecFormat::INT24_RJ:
        return get_sample_converter<RaspaCodecFormat::INT24_RJ>
                (buffer_size_in_frames, num_channels);

    case RaspaCodecFormat::INT32_RJ:
        return get_sample_converter<RaspaCodecFormat::INT32_RJ>
                (buffer_size_in_frames, num_channels);

    case RaspaCodecFormat::NUM_CODEC_FORMATS:
        // never used. Implemented here to suppress warnings
        break;
    }

    return std::make_unique<SampleConverterGeneric>(codec_format,
                                                    buffer_size_in_frames,
                                                    num_channels);
}

} // namespace raspa

#endif // SAMPLE_CONVERSION_H_