/**
 * @brief Utilities functions re-used by unit tests
 */

#ifndef RASPA_TEST_UTILS_H
#define RASPA_TEST_UTILS_H

constexpr int RASPA_INT24_LJ_MAX_VALUE = 0x7FFFFF00;
constexpr int RASPA_INT24_LJ_MIN_VALUE = 0x80000000;

constexpr int RASPA_INT24_I2S_MAX_VALUE = 0x3FFFFF80;
constexpr int RASPA_INT24_I2S_MIN_VALUE = 0x40000000;

constexpr int RASPA_INT24_RJ_MAX_VALUE = 0x007FFFFF;
constexpr int RASPA_INT24_RJ_MIN_VALUE = 0x00800000;

constexpr int RASPA_INT24_32RJ_MAX_VALUE = 0x007FFFFF;
constexpr int RASPA_INT24_32RJ_MIN_VALUE = 0xFF800000;

constexpr int RASPA_INT32_MAX_VALUE = 0x7FFFFFFF;
constexpr int RASPA_INT32_MIN_VALUE = 0x80000000;

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

inline void assert_buffer_value_int_near(int32_t value, int32_t abs_error, int32_t* buf, int buffer_size_in_samples)
{
    for (int i=0; i < buffer_size_in_samples; i++)
    {
        ASSERT_NEAR(value, buf[i], abs_error);
    }
}

#endif // RASPA_TEST_UTILS_H
