/**
 * @brief Utilities functions re-used by unit tests
 */

#ifndef RASPA_TEST_UTILS_H
#define RASPA_TEST_UTILS_H

constexpr int RASPA_INT24_MAX_VALUE = 8388607;
constexpr int RASPA_INT24_MIN_VALUE = -8388607;

#define MAX_ALLOWED_ABS_ERROR (1.0e-6)

inline void assert_buffers_equal(float* buf_a, float* buf_b, int buffer_size_in_samples)
{
    for (int i=0; i < buffer_size_in_samples; i++)
    {
        ASSERT_NEAR(buf_a[i], buf_b[i], MAX_ALLOWED_ABS_ERROR);
    }
}

inline void assert_buffers_equal_int(int32_t* buf_a, int32_t* buf_b, int buffer_size_in_samples)
{
    for (int i=0; i < buffer_size_in_samples; i++)
    {
        ASSERT_EQ(buf_a[i], buf_b[i]);
    }
}

inline void assert_buffer_value(float value, float* buf, int buffer_size_in_samples)
{
    for (int i=0; i < buffer_size_in_samples; i++)
    {
        ASSERT_FLOAT_EQ(value, buf[i]);
    }
}

inline void assert_buffer_value_int(int32_t value, int32_t* buf, int buffer_size_in_samples)
{
    for (int i=0; i < buffer_size_in_samples; i++)
    {
        ASSERT_EQ(value, buf[i]);
    }
}

#endif // RASPA_TEST_UTILS_H
