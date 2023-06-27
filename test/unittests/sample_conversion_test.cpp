#include <algorithm>
#include <vector>

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include "sample_conversion.h"
#include "test_utils.h"
#include "driver_config.h"

#include <math.h>

class TestSampleConversion : public ::testing::Test
{
protected:
    TestSampleConversion()
    {
    }

    void SetUp()
    {}

    void TearDown()
    {}

    void _init_data_ramp_float(std::vector<float> &buffer,
                               int buffer_size_in_samples)
    {
        buffer.resize(buffer_size_in_samples, 0);

        int sample_index = 0;
        for (auto &sample : buffer)
        {
            sample = -1.0f + (2.0f / buffer_size_in_samples) *
                             static_cast<float>(sample_index);
            sample_index++;
        }
    }

    void _init_data_ramp_int(std::vector<int32_t> &buffer,
                             int buffer_size_in_samples,
                             driver_conf::CodecFormat codec_format)
    {
        buffer.resize(buffer_size_in_samples, 0);

        int sample_index = 0;
        for (auto &sample : buffer)
        {
            int32_t value = sample_index * 100;
            switch (codec_format)
            {
            case driver_conf::CodecFormat::INT24_LJ:
                value = (value & 0x00FFFFFF) << 8;
                break;
            case driver_conf::CodecFormat::INT24_I2S:
                value = (value & 0x00FFFFFF) << 7;
                break;
            case driver_conf::CodecFormat::INT24_RJ:
                value = (value & 0x00FFFFFF);
                break;
            case driver_conf::CodecFormat::INT24_32RJ:
                // represents 24 bit of data in 32bit right justified format
                value = (value & 0x00FFFFFF) << 8;

                // extend sign
                value = value >> 8;
                break;
            case driver_conf::CodecFormat::INT32:
                // No conversion needed
                break;
            }
            sample = value;
            sample_index++;
        }
    }

    int get_codec_min(enum driver_conf::CodecFormat format)
    {
        switch (format)
        {
            case driver_conf::CodecFormat::INT24_LJ:
                return RASPA_INT24_LJ_MIN_VALUE;
            case driver_conf::CodecFormat::INT24_I2S:
                return RASPA_INT24_I2S_MIN_VALUE;
            case driver_conf::CodecFormat::INT24_RJ:
                return RASPA_INT24_RJ_MIN_VALUE;
            case driver_conf::CodecFormat::INT24_32RJ:
                return RASPA_INT24_32RJ_MIN_VALUE;
            case driver_conf::CodecFormat::INT32:
                return RASPA_INT32_MIN_VALUE;
            default:
                break;
        }
        return 0;
    }

    int get_codec_max(enum driver_conf::CodecFormat format)
    {
        switch (format)
        {
            case driver_conf::CodecFormat::INT24_LJ:
                return RASPA_INT24_LJ_MAX_VALUE;
            case driver_conf::CodecFormat::INT24_I2S:
                return RASPA_INT24_I2S_MAX_VALUE;
            case driver_conf::CodecFormat::INT24_RJ:
                return RASPA_INT24_RJ_MAX_VALUE;
            case driver_conf::CodecFormat::INT24_32RJ:
                return RASPA_INT24_32RJ_MAX_VALUE;
            case driver_conf::CodecFormat::INT32:
                return RASPA_INT32_MAX_VALUE;
            default:
                break;
        }
        return 0;
    }

    std::vector<enum driver_conf::CodecFormat> test_codec_formats = 
    {
        driver_conf::CodecFormat::INT24_LJ,
        driver_conf::CodecFormat::INT24_I2S,
        driver_conf::CodecFormat::INT24_RJ,
        driver_conf::CodecFormat::INT24_32RJ,
        driver_conf::CodecFormat::INT32
    };
};

TEST_F(TestSampleConversion, invalid_audio_parameters)
{
    int sw_chan = 0;
    int buffer_size = 64;
    int invalid_buffer_size = 63;
    auto codec_format = driver_conf::CodecFormat::INT24_LJ;
    auto invalid_codec_format = driver_conf::CodecFormat::NUM_CODEC_FORMATS;
    int stride = 2;
    int invalid_stride = 3;

    // invalid buffer size
    auto sample_converter = raspa::get_sample_converter(sw_chan,
                                                        invalid_buffer_size,
                                                        codec_format,
                                                        0,
                                                        stride);
    ASSERT_FALSE(sample_converter);

    // invalid codec format
    sample_converter = raspa::get_sample_converter(sw_chan,
                                                   buffer_size,
                                                   invalid_codec_format,
                                                   0,
                                                   stride);
    ASSERT_FALSE(sample_converter);

    // invalid stride
    sample_converter = raspa::get_sample_converter(sw_chan,
                                                   buffer_size,
                                                   codec_format,
                                                   0,
                                                   invalid_stride);
    ASSERT_FALSE(sample_converter);
}

TEST_F(TestSampleConversion, identity_conversion_float_int_float)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;
    std::vector<float> expected_float_data;

    for (auto codec_format : test_codec_formats)
    {
        for (auto buffer_size : raspa::SUPPORTED_BUFFER_SIZES)
        {
            for (auto stride : raspa::SUPPORTED_STRIDES)
            {
                auto total_buffer_size = buffer_size * stride;

                // resize buffers
                int_data.resize(total_buffer_size, 0);
                float_data.resize(total_buffer_size, 0);

                // test conversion
                _init_data_ramp_float(expected_float_data, total_buffer_size);

                for (int channel = 0; channel < stride; channel++)
                {
                    // we are testing sw.channel = hw.channel only
                    auto sample_converter = raspa::get_sample_converter(
                                                channel,
                                                buffer_size,
                                                codec_format,
                                                channel,
                                                stride);
                    ASSERT_TRUE(sample_converter);

                    sample_converter->float32n_to_codec_format(
                                        int_data.data(),
                                        expected_float_data.data());

                    sample_converter->codec_format_to_float32n(
                                        float_data.data(),
                                        int_data.data());
                }

                assert_buffers_equal(expected_float_data.data(),
                                     float_data.data(),
                                     total_buffer_size);
            }
        }
    }
}

TEST_F(TestSampleConversion, identity_conversion_int_float_int)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;
    std::vector<int32_t> expected_int_data;

    for (auto codec_format : test_codec_formats)
    {
        for (auto buffer_size : raspa::SUPPORTED_BUFFER_SIZES)
        {
            for (auto stride : raspa::SUPPORTED_STRIDES)
            {
                auto total_buffer_size = buffer_size * stride;

                // resize buffers
                int_data.resize(total_buffer_size, 0);
                float_data.resize(total_buffer_size, 0);

                // test conversion
                _init_data_ramp_int(expected_int_data,
                                    total_buffer_size,
                                    codec_format);

                for (int channel = 0; channel < stride; channel++)
                {
                    // we are testing sw.channel = hw.channel only
                    auto sample_converter = raspa::get_sample_converter(
                                                channel,
                                                buffer_size,
                                                codec_format,
                                                channel,
                                                stride);
                    ASSERT_TRUE(sample_converter);

                    sample_converter->codec_format_to_float32n(
                        float_data.data(),
                        expected_int_data.data());

                    sample_converter->float32n_to_codec_format(
                        int_data.data(),
                        float_data.data());
                }

                assert_buffers_equal_int(
                    expected_int_data.data(),
                    int_data.data(),
                    total_buffer_size);
            }
        }
    }
}

TEST_F(TestSampleConversion, test_clipping_max)
{
    const int32_t i32_max_clip_error = 0xFF;
    std::vector<int32_t> int_data;
    std::vector<float> float_data;

    // First of all we test something explicit for the INT32 positive clipping
    ASSERT_NEAR(INT32_MAX, INT32_MAX, i32_max_clip_error);
    ASSERT_NEAR(INT32_MAX, 0x7FFFFF00, i32_max_clip_error);
    EXPECT_FATAL_FAILURE(
        ASSERT_NEAR(INT32_MAX, 0x7FFFFF00 - 1, i32_max_clip_error),
        "" // match all assert messages
    );
    EXPECT_FATAL_FAILURE(
        ASSERT_NEAR(INT32_MAX, INT32_MIN, i32_max_clip_error),
        "" // match all assert messages
    );

    for (auto codec_format : test_codec_formats)
    {
        for (auto buffer_size : raspa::SUPPORTED_BUFFER_SIZES)
        {
            for (auto stride : raspa::SUPPORTED_STRIDES)
            {
                auto total_buffer_size = buffer_size * stride;

                int_data.resize(total_buffer_size, 0);
                float_data.resize(total_buffer_size, 2.0f);

                for (int channel = 0; channel < stride; channel++)
                {
                    // we are testing sw.channel = hw.channel only
                    auto sample_converter = raspa::get_sample_converter(
                                                channel,
                                                buffer_size,
                                                codec_format,
                                                channel,
                                                stride);
                    ASSERT_TRUE(sample_converter);

                    sample_converter->float32n_to_codec_format(
                        int_data.data(),
                        float_data.data());
                }

                if (codec_format != driver_conf::CodecFormat::INT32)
                {
                    assert_buffer_value_int(get_codec_max(codec_format),
                                            int_data.data(),
                                            total_buffer_size);
                }
                else
                {
                    // handle float +1 clipping exception:
                    assert_buffer_value_int_near(get_codec_max(codec_format),
                                                 i32_max_clip_error,
                                                 int_data.data(),
                                                 total_buffer_size);
                }
            }
        }
    }
}

TEST_F(TestSampleConversion, test_clipping_min)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;

    for (auto codec_format : test_codec_formats)
    {
        for (auto buffer_size : raspa::SUPPORTED_BUFFER_SIZES)
        {
            for (auto stride : raspa::SUPPORTED_STRIDES)
            {
                auto total_buffer_size = buffer_size * stride;

                int_data.resize(total_buffer_size, 0);
                float_data.resize(total_buffer_size, -2.0f);

                for (int channel = 0; channel < stride; channel++)
                {
                    // we are testing sw.channel = hw.channel only
                    auto sample_converter = raspa::get_sample_converter(
                                                channel,
                                                buffer_size,
                                                codec_format,
                                                channel,
                                                stride);
                    ASSERT_TRUE(sample_converter);

                    sample_converter->float32n_to_codec_format(
                        int_data.data(),
                        float_data.data());
                }

                assert_buffer_value_int(get_codec_min(codec_format),
                                        int_data.data(),
                                        total_buffer_size);
            }
        }
    }
}

TEST_F(TestSampleConversion, test_zero_conversion_float_int)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;

    for (auto codec_format : test_codec_formats)
    {
        for (auto buffer_size : raspa::SUPPORTED_BUFFER_SIZES)
        {
            for (auto stride : raspa::SUPPORTED_STRIDES)
            {
                int total_buffer_size = buffer_size * stride;

                // fill int buffer with wrong values
                int_data.resize(total_buffer_size, 10);
                float_data.resize(total_buffer_size, 0.0f);

                for (int channel = 0; channel < stride; channel++)
                {
                    // we are testing sw.channel = hw.channel only
                    auto sample_converter = raspa::get_sample_converter(
                                                channel,
                                                buffer_size,
                                                codec_format,
                                                channel,
                                                stride);
                    ASSERT_TRUE(sample_converter);

                    sample_converter->float32n_to_codec_format(int_data.data(),
                                                            float_data.data());
                }

                assert_buffer_value_int(0, int_data.data(),
                                        total_buffer_size);
            }
        }
    }
}

TEST_F(TestSampleConversion, test_zero_conversion_int_float)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;

    for (auto codec_format : test_codec_formats)
    {
        for (auto buffer_size : raspa::SUPPORTED_BUFFER_SIZES)
        {
            for (auto stride : raspa::SUPPORTED_STRIDES)
            {
                int total_buffer_size = buffer_size * stride;

                // fill float buffer with wrong values
                int_data.resize(total_buffer_size, 0);
                float_data.resize(total_buffer_size, 0.5f);

                for (int channel = 0; channel < stride; channel++)
                {
                    // we are testing sw.channel = hw.channel only
                    auto sample_converter = raspa::get_sample_converter(
                                                channel,
                                                buffer_size,
                                                codec_format,
                                                channel,
                                                stride);
                    ASSERT_TRUE(sample_converter);

                    sample_converter->codec_format_to_float32n(
                                            float_data.data(),
                                            int_data.data());
                }

                assert_buffer_value(0.0f, float_data.data(),
                                    total_buffer_size);
            }
        }
    }
}