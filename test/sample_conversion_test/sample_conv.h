#ifndef RASPALIB_SAMPLE_CONV_H
#define RASPALIB_SAMPLE_CONV_H

void int2float_var(float* dst, int32_t* src, int buffer_size);

void int2float(float* dst, int32_t* src, int buffer_size);

void int2float_b8(float* dst, int32_t* src);

void int2float_b16(float* dst, int32_t* src);

void int2float_b32(float* dst, int32_t* src);

void int2float_b64(float* dst, int32_t* src);

void int2float_b128(float* dst, int32_t* src);

void int2float_b256(float* dst, int32_t* src);

void int2float_b512(float* dst, int32_t* src);

void float2int_var(int32_t* dst, float* src, int buffer_size);

void float2int(int32_t* dst, float* src , int buffer_size);

void float2int_b8(int32_t* dst, float* src);

void float2int_b16(int32_t* dst, float* src);

void float2int_b32(int32_t* dst, float* src);

void float2int_b64(int32_t* dst, float* src);

void float2int_b128(int32_t* dst, float* src);

void float2int_b256(int32_t* dst, float* src);

void float2int_b512(int32_t* dst, float* src);

#endif //RASPALIB_SAMPLE_CONV_H
