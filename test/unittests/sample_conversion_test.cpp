#include <algorithm>

#include "gtest/gtest.h"

#include "sample_conversion.h"
#include "test_utils.h"

class TestSampleConversion : public ::testing::Test
{
protected:
    TestSampleConversion()
    {
    }

    void SetUp()
    {
        // setup for maximum possible buffer size

        // Prepare test data buffers
        for (int i=0; i < RASPA_MAX_N_SAMPLES_PER_BUFFER; i++)
        {
            // ramp -1.0 .. 1.0
            _data_ramp[i] = -1.0f + (2.0f/RASPA_MAX_N_SAMPLES_PER_BUFFER) * static_cast<float>(i);
            // Not the same ramp
            int value = i * 100;
            value = (value & 0x00FFFFFF) << 8;

            _data_ramp_int_lj[i] = value;
        }
    }

    void TearDown()
    {
    }

    float   _data_ramp[RASPA_MAX_N_SAMPLES_PER_BUFFER];
    int32_t _data_ramp_int_lj[RASPA_MAX_N_SAMPLES_PER_BUFFER];
};

TEST_F(TestSampleConversion, identity_conversion_float_int_float_lj)
{
    int32_t int_data[RASPA_MAX_N_SAMPLES_PER_BUFFER];
    float float_data[RASPA_MAX_N_SAMPLES_PER_BUFFER];

    for(int buffer_size = RASPA_MIN_N_FRAMES_PER_BUFFER;
        buffer_size <= RASPA_MAX_N_FRAMES_PER_BUFFER;
        buffer_size = buffer_size * 2)
    {
        std::fill(int_data, int_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 0);
        std::fill(float_data, float_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 0.0f);

        RASPA_FLOAT_TO_INT(int_data, _data_ramp, buffer_size);
        RASPA_INT_TO_FLOAT(float_data, int_data, buffer_size);
        assert_buffers_equal(_data_ramp, float_data, buffer_size);
    }
}

TEST_F(TestSampleConversion, identity_conversion_int_float_int_lj)
{
    int32_t int_data[RASPA_MAX_N_SAMPLES_PER_BUFFER];
    float float_data[RASPA_MAX_N_SAMPLES_PER_BUFFER];

    for(int buffer_size = RASPA_MIN_N_FRAMES_PER_BUFFER;
        buffer_size <= RASPA_MAX_N_FRAMES_PER_BUFFER;
        buffer_size = buffer_size * 2)
    {
        std::fill(int_data, int_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 0);
        std::fill(float_data, float_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 0.0f);

        RASPA_INT_TO_FLOAT(float_data, _data_ramp_int_lj, buffer_size);
        RASPA_FLOAT_TO_INT(int_data, float_data, buffer_size);
        assert_buffers_equal_int(_data_ramp_int_lj, int_data, buffer_size);
    }
}

TEST_F(TestSampleConversion, test_clipping)
{
    float float_data[RASPA_MAX_N_SAMPLES_PER_BUFFER];
    int32_t int_data[RASPA_MAX_N_SAMPLES_PER_BUFFER];

    // Above 1.0f
    std::fill(float_data, float_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 2.0f);

    for(int buffer_size = RASPA_MIN_N_FRAMES_PER_BUFFER;
        buffer_size <= RASPA_MAX_N_FRAMES_PER_BUFFER;
        buffer_size = buffer_size * 2)
    {
        std::fill(int_data, int_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 0);

        RASPA_FLOAT_TO_INT(int_data, float_data, buffer_size);
        assert_buffer_value_int(INT24_LJ32_MAX_VALUE, int_data, buffer_size);
    }

    // Belove -1.0f
    std::fill(float_data, float_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, -3.0f);
    for(int buffer_size = RASPA_MIN_N_FRAMES_PER_BUFFER;
        buffer_size <= RASPA_MAX_N_FRAMES_PER_BUFFER;
        buffer_size = buffer_size * 2)
    {
        std::fill(int_data, int_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 0);

        RASPA_FLOAT_TO_INT(int_data, float_data, buffer_size);
        assert_buffer_value_int(INT24_LJ32_MIN_VALUE, int_data, buffer_size);
    }

}

TEST_F(TestSampleConversion, test_zero_conversion)
{
    float float_data[RASPA_MAX_N_SAMPLES_PER_BUFFER];
    int32_t int_data[RASPA_MAX_N_SAMPLES_PER_BUFFER];

    // Float -> Int
    std::fill(float_data, float_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 0.0f);

    for(int buffer_size = RASPA_MIN_N_FRAMES_PER_BUFFER;
    buffer_size <= RASPA_MAX_N_FRAMES_PER_BUFFER;
    buffer_size = buffer_size * 2)
    {
        // fill with wrong vals
        std::fill(int_data, int_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 10);

        RASPA_FLOAT_TO_INT(int_data, float_data, buffer_size);
        assert_buffer_value_int(0, int_data, buffer_size);
    }

    // Int -> Float
    std::fill(int_data, int_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 0);

    for(int buffer_size = RASPA_MIN_N_FRAMES_PER_BUFFER;
    buffer_size <= RASPA_MAX_N_FRAMES_PER_BUFFER;
    buffer_size = buffer_size * 2)
    {
        // fill with wrong vals
        std::fill(float_data, float_data + RASPA_MAX_N_SAMPLES_PER_BUFFER, 0.5f);

        RASPA_INT_TO_FLOAT(float_data, int_data, buffer_size);
        assert_buffer_value(0.0f, float_data, buffer_size);
    }
}
