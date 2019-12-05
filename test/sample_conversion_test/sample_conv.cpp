#include <cstdint>
#include "sample_conv.h"

#include "sample_conversion/sample_conversion.h"

void int2float(float* dst, int32_t* src, int size)
{
    int24lj32i_to_float32n(dst, src, size);
}

void int2float_b8(float* dst, int32_t* src)
{
    int24lj32i_to_float32n_b8(dst, src);
}

void int2float_b16(float* dst, int32_t* src)
{
    int24lj32i_to_float32n_b16(dst, src);
}

void int2float_b32(float* dst, int32_t* src)
{
    int24lj32i_to_float32n_b32(dst, src);
}

void int2float_b64(float* dst, int32_t* src)
{
    int24lj32i_to_float32n_b64(dst, src);
}

void int2float_b128(float* dst, int32_t* src)
{
    int24lj32i_to_float32n_b128(dst, src);
}

void int2float_b256(float* dst, int32_t* src)
{
    int24lj32i_to_float32n_b256(dst, src);
}

void int2float_b512(float* dst, int32_t* src)
{
    int24lj32i_to_float32n_b512(dst, src);
}

void int2float_var(float* dst, int32_t* src, int buffer_size)
{
    int n, k;
    for (n = 0; n < buffer_size; n++)
    {
        for (k = 0; k < RASPA_N_TOTAL_CHANNELS; k++)
        {
            int32_t x = *src++;
            float y = sample_int24lj32_to_float32(x);
            dst[(k * buffer_size) + n] = y;
        }
    }
}

void float2int_var(int32_t* dst, float* src, int buffer_size)
{
    int n, k;
    for (k = 0; k < RASPA_N_TOTAL_CHANNELS; k++)
    {
        for (n = 0; n < buffer_size; n++)
        {
            float x = *src++;
            int32_t y = sample_float32_to_int24lj32(x);
            dst[n * RASPA_N_TOTAL_CHANNELS + k] = y;
        }
    }
}

void float2int(int32_t* dst, float* src , int buffer_size)
{
    float32n_to_int24lj32i(dst, src, buffer_size);
}

void float2int_b8(int32_t* dst, float* src)
{
    float32n_to_int24lj32i_b8(dst, src);
}

void float2int_b16(int32_t* dst, float* src)
{
    float32n_to_int24lj32i_b16(dst, src);
}

void float2int_b32(int32_t* dst, float* src)
{
    float32n_to_int24lj32i_b32(dst, src);
}

void float2int_b64(int32_t* dst, float* src)
{
    float32n_to_int24lj32i_b64(dst, src);
}

void float2int_b128(int32_t* dst, float* src)
{
    float32n_to_int24lj32i_b128(dst, src);
}

void float2int_b256(int32_t* dst, float* src)
{
    float32n_to_int24lj32i_b256(dst, src);
}

void float2int_b512(int32_t* dst, float* src)
{
    float32n_to_int24lj32i_b512(dst, src);
}