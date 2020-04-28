#include <algorithm>
#include <vector>

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

TEST_F(TestSampleConversion, invalid_audio_parameters)
{
    RaspaCodecFormat codec_format = RaspaCodecFormat::NUM_CODEC_FORMATS;
    int num_chans = raspa::MIN_NUM_CHANNELS;
    int buffer_size = raspa::MIN_BUFFER_SIZE;

    // invalid codec format
    auto sample_converter = raspa::get_sample_converter(codec_format,
                                                        buffer_size, num_chans);
    ASSERT_FALSE(sample_converter);

    // invalid number of channels
    codec_format = RaspaCodecFormat::INT24_LJ;
    num_chans = raspa::MAX_NUM_CHANNELS * 2;
    sample_converter = raspa::get_sample_converter(codec_format,
                                                   buffer_size, num_chans);
    ASSERT_FALSE(sample_converter);

    // invalid buffer size of channels
    num_chans = raspa::MAX_NUM_CHANNELS;
    buffer_size = raspa::MAX_BUFFER_SIZE * 2;
    sample_converter = raspa::get_sample_converter(codec_format,
                                                   buffer_size, num_chans);
    ASSERT_FALSE(sample_converter);
}

TEST_F(TestSampleConversion, identity_conversion_float_int_float_lj)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;
    std::vector<float> expected_float_data;

    RaspaCodecFormat codec_format = RaspaCodecFormat::INT24_LJ;
    bool iterate_over_codec_format = true;

    while (iterate_over_codec_format)
    {
        int num_chans = raspa::MIN_NUM_CHANNELS;
        bool iterate_over_channels = true;

        while (iterate_over_channels)
        {
            int buffer_size = raspa::MIN_BUFFER_SIZE;
            bool iterate_over_buffer_size = true;

            while (iterate_over_buffer_size)
            {
                int buffer_size_in_samples = buffer_size * num_chans;

                int_data.resize(buffer_size_in_samples, 0);
                float_data.resize(buffer_size_in_samples, 0);
                _init_data_ramp_float(expected_float_data,
                                      buffer_size_in_samples);

                auto sample_converter = raspa::get_sample_converter(codec_format,
                        buffer_size, num_chans);

                ASSERT_TRUE(sample_converter);

                sample_converter->float32n_to_codec_format(int_data.data(),
                                                           expected_float_data.data());
                sample_converter->codec_format_to_float32n(float_data.data(),
                                                           int_data.data());
                assert_buffers_equal(expected_float_data.data(),
                                     float_data.data(), buffer_size_in_samples);

                auto next_buffer_size = raspa::get_next_buffer_size(buffer_size);
                iterate_over_buffer_size = next_buffer_size.first;
                buffer_size = next_buffer_size.second;
            }

            auto next_num_chans = raspa::get_next_num_channels(num_chans);
            iterate_over_channels = next_num_chans.first;
            num_chans = next_num_chans.second;
        }

        auto next_codec_format = raspa::get_next_codec_format(codec_format);
        iterate_over_codec_format = next_codec_format.first;
        codec_format = next_codec_format.second;
    }
}

TEST_F(TestSampleConversion, identity_conversion_int_float_int_lj)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;
    std::vector<int32_t> expected_int_data;

    RaspaCodecFormat codec_format = RaspaCodecFormat::INT24_LJ;
    bool iterate_over_codec_format = true;

    while (iterate_over_codec_format)
    {
        int num_chans = raspa::MIN_NUM_CHANNELS;
        bool iterate_over_channels = true;

        while (iterate_over_channels)
        {
            int buffer_size = raspa::MIN_BUFFER_SIZE;
            bool iterate_over_buffer_size = true;

            while (iterate_over_buffer_size)
            {
                int buffer_size_in_samples = buffer_size * num_chans;

                int_data.resize(buffer_size_in_samples, 0);
                float_data.resize(buffer_size_in_samples, 0);
                _init_data_ramp_int(expected_int_data,
                                    buffer_size_in_samples,
                                    codec_format);

                auto sample_converter = raspa::get_sample_converter(codec_format,
                        buffer_size,
                        num_chans);

                ASSERT_TRUE(sample_converter);

                sample_converter->codec_format_to_float32n(float_data.data(),
                                                           expected_int_data.data());
                sample_converter->float32n_to_codec_format(int_data.data(),
                                                           float_data.data());
                assert_buffers_equal_int(expected_int_data.data(),
                                         int_data.data(),
                                         buffer_size_in_samples);

                auto next_buffer_size = raspa::get_next_buffer_size(buffer_size);
                iterate_over_buffer_size = next_buffer_size.first;
                buffer_size = next_buffer_size.second;
            }

            auto next_num_chans = raspa::get_next_num_channels(num_chans);
            iterate_over_channels = next_num_chans.first;
            num_chans = next_num_chans.second;
        }

        auto next_codec_format = raspa::get_next_codec_format(codec_format);
        iterate_over_codec_format = next_codec_format.first;
        codec_format = next_codec_format.second;
    }
}

TEST_F(TestSampleConversion, test_clipping)
{
    std::vector<int32_t> int_data;
    std::vector<float> float_data;

    int buffer_size_in_frames = raspa::MAX_BUFFER_SIZE;
    int num_chans = raspa::MAX_NUM_CHANNELS;
    RaspaCodecFormat codec_format = raspa::DEFAULT_CODEC_FORMAT;
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

    RaspaCodecFormat codec_format = RaspaCodecFormat::INT24_LJ;
    bool iterate_over_codec_format = true;

    while (iterate_over_codec_format)
    {
        int num_chans = raspa::MIN_NUM_CHANNELS;
        bool iterate_over_channels = true;

        while (iterate_over_channels)
        {
            int buffer_size = raspa::MIN_BUFFER_SIZE;
            bool iterate_over_buffer_size = true;

            while (iterate_over_buffer_size)
            {
                int buffer_size_in_samples = buffer_size * num_chans;

                // fill int buffer with wrong values
                int_data.resize(buffer_size_in_samples, 10);
                float_data.resize(buffer_size_in_samples, 0.0f);

                auto sample_converter = raspa::get_sample_converter(codec_format,
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

                auto next_buffer_size = raspa::get_next_buffer_size(buffer_size);
                iterate_over_buffer_size = next_buffer_size.first;
                buffer_size = next_buffer_size.second;
            }

            auto next_num_chans = raspa::get_next_num_channels(num_chans);
            iterate_over_channels = next_num_chans.first;
            num_chans = next_num_chans.second;
        }

        auto next_codec_format = raspa::get_next_codec_format(codec_format);
        iterate_over_codec_format = next_codec_format.first;
        codec_format = next_codec_format.second;
    }
}
