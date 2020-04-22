#include <algorithm>
#include <vector>

#include <iostream>

#include "gtest/gtest.h"

#include "sample_conversion.h"
#include "test_utils.h"
#include "driver_config.h"

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
                             RaspaCodecFormat codec_format)
    {
        buffer.resize(buffer_size_in_samples, 0);

        int sample_index = 0;
        for (auto &sample : buffer)
        {
            int32_t value = sample_index * 100;
            switch (codec_format)
            {
            case RaspaCodecFormat::INT24_LJ:
                value = (value & 0x00FFFFFF) << 8;
                break;
            case RaspaCodecFormat::INT24_I2S:
                value = (value & 0x00FFFFFF) << 7;
                break;
            case RaspaCodecFormat::INT24_RJ:
                value = (value & 0x00FFFFFF);
                break;
            case RaspaCodecFormat::INT32_RJ:
                // represents 24 bit of data in 32bit right justified format
                value = (value & 0x00FFFFFF) << 8;

                // extend sign
                value = value >> 8;
                break;
            }
            sample = value;
            sample_index++;
        }
    }
};

TEST_F(TestSampleConversion, identity_conversion_float_int_float_lj)
{

    std::vector<int32_t> int_data;
    std::vector<float> float_data;
    std::vector<float> expected_float_data;

    for (int codec_format = static_cast<int>(RaspaCodecFormat::INT24_LJ);
         codec_format < static_cast<int>(RaspaCodecFormat::NUM_CODEC_FORMATS);
         codec_format++)
    {
        for (int num_chans = RASPA_MIN_NUM_CHANNELS;
             num_chans < RASPA_MAX_NUM_CHANNELS; num_chans++)
        {
            for (int buffer_size = RASPA_MIN_BUFFER_SIZE_IN_FRAMES;
                 buffer_size <
                 RASPA_MAX_BUFFER_SIZE_IN_FRAMES; buffer_size += 8)
            {
                int buffer_size_in_samples = buffer_size * num_chans;

                int_data.resize(buffer_size_in_samples, 0);
                float_data.resize(buffer_size_in_samples, 0);
                _init_data_ramp_float(expected_float_data,
                                      buffer_size_in_samples);

                auto sample_converter = raspa::get_sample_converter(
                        static_cast<RaspaCodecFormat>(codec_format),
                        buffer_size, num_chans);

                sample_converter->float32n_to_codec_format(int_data.data(),
                                                           expected_float_data.data());
                sample_converter->codec_format_to_float32n(float_data.data(),
                                                           int_data.data());
                assert_buffers_equal(expected_float_data.data(),
                                     float_data.data(), buffer_size_in_samples);
            }
        }
    }
}

TEST_F(TestSampleConversion, identity_conversion_int_float_int_lj)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;
    std::vector<int32_t> expected_int_data;

    for (int codec_format = static_cast<int>(RaspaCodecFormat::INT24_LJ);
         codec_format < static_cast<int>(RaspaCodecFormat::NUM_CODEC_FORMATS);
         codec_format++)
    {
        for (int num_chans = RASPA_MIN_NUM_CHANNELS;
             num_chans < RASPA_MAX_NUM_CHANNELS; num_chans++)
        {
            for (int buffer_size = RASPA_MIN_BUFFER_SIZE_IN_FRAMES;
                 buffer_size < RASPA_MAX_BUFFER_SIZE_IN_FRAMES;
                 buffer_size += 8)
            {
                int buffer_size_in_samples = buffer_size * num_chans;

                int_data.resize(buffer_size_in_samples, 0);
                float_data.resize(buffer_size_in_samples, 0);
                _init_data_ramp_int(expected_int_data,
                                    buffer_size_in_samples,
                                    static_cast<RaspaCodecFormat>(codec_format));

                auto sample_converter = raspa::get_sample_converter(
                        static_cast<RaspaCodecFormat>(codec_format),
                        buffer_size,
                        num_chans);

                sample_converter->codec_format_to_float32n(float_data.data(),
                                                           expected_int_data.data());
                sample_converter->float32n_to_codec_format(int_data.data(),
                                                           float_data.data());
                assert_buffers_equal_int(expected_int_data.data(),
                                         int_data.data(),
                                         buffer_size_in_samples);
            }
        }
    }
}

TEST_F(TestSampleConversion, test_clipping)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;

    int buffer_size_in_frames = 512;
    int num_chans = 8;
    RaspaCodecFormat codec_format = RaspaCodecFormat::INT24_LJ;
    int buffer_size_in_samples = buffer_size_in_frames * num_chans;

    int32_t int24_lj_max_val = (RASPA_INT24_MAX_VALUE << 8);
    int32_t int24_lj_min_val = (RASPA_INT24_MIN_VALUE << 8);

    int_data.resize(buffer_size_in_samples, 0);
    float_data.resize(buffer_size_in_samples, 2.0);

    auto sample_converter = raspa::get_sample_converter(codec_format,
                                                        buffer_size_in_frames,
                                                        num_chans);
    sample_converter->float32n_to_codec_format(int_data.data(),
                                               float_data.data());
    assert_buffer_value_int(int24_lj_max_val, int_data.data(),
                            buffer_size_in_samples);

    std::fill(float_data.begin(), float_data.end(), -2.0f);
    std::fill(int_data.begin(), int_data.end(), 0);
    sample_converter->float32n_to_codec_format(int_data.data(),
                                               float_data.data());
    assert_buffer_value_int(int24_lj_min_val, int_data.data(),
                            buffer_size_in_samples);
}

TEST_F(TestSampleConversion, test_zero_conversion)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;

    for (int codec_format = static_cast<int>(RaspaCodecFormat::INT24_LJ);
         codec_format < static_cast<int>(RaspaCodecFormat::NUM_CODEC_FORMATS);
         codec_format++)
    {
        for (int num_chans = RASPA_MIN_NUM_CHANNELS;
             num_chans < RASPA_MAX_NUM_CHANNELS; num_chans++)
        {
            for (int buffer_size = RASPA_MIN_BUFFER_SIZE_IN_FRAMES;
                 buffer_size < RASPA_MAX_BUFFER_SIZE_IN_FRAMES;
                 buffer_size += 8)
            {
                int buffer_size_in_samples = buffer_size * num_chans;

                // fill int buffer with wrong values
                int_data.resize(buffer_size_in_samples, 10);
                float_data.resize(buffer_size_in_samples, 0.0f);

                auto sample_converter = raspa::get_sample_converter(
                        static_cast<RaspaCodecFormat>(codec_format),
                        buffer_size, num_chans);

                sample_converter->float32n_to_codec_format(int_data.data(),
                                                           float_data.data());
                assert_buffer_value_int(0, int_data.data(),
                                        buffer_size_in_samples);

                // fill float buffer with wrong values
                std::fill(float_data.begin(), float_data.end(), 0.5f);
                std::fill(int_data.begin(), int_data.end(), 0);
                sample_converter->codec_format_to_float32n(float_data.data(),
                                                           int_data.data());
                assert_buffer_value(0.0f, float_data.data(),
                                    buffer_size_in_samples);
            }
        }
    }
}
