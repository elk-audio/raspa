#ifndef SAMPLE_CONVERTER_GENERIC_H_
#define SAMPLE_CONVERTER_GENERIC_H_

#include "sample_conversion.h"

namespace raspa {

/**
 * @brief Class which performs sample conversion for any generic codec format,
 *        buffer sizes or number of channels. This will not be optimized as the
 *        number of iterations in the inner and outer loops are determined at
 *        run time
 */
class SampleConverterGeneric : public BaseSampleConverter
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

                auto sample = (int32_t)(x * FLOAT_TO_INT24_SCALING_FACTOR);
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

}
#endif