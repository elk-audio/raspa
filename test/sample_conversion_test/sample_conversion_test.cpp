#include <chrono>
#include <iostream>
#include <array>
#include <atomic>

#include "common_defs.h"

#include "sample_conv.h"

constexpr int ITERATIONS   = 500;
constexpr int SUB_ITERATIONS = 200;

float float_rand(float min, float max)
{
    return (((std::rand()) * (max - min)) / static_cast<float>(RAND_MAX)) + min;
}

template <size_t size>
void fill_buffer(std::array<float, size>& buffer, float peak_value)
{
    for (auto& sample : buffer)
    {
        sample = float_rand(-peak_value, peak_value);
    }
}

struct Results
{
    std::chrono::nanoseconds fixed;
    std::chrono::nanoseconds semi;
    std::chrono::nanoseconds variable;
};

void print_results(Results results, int buffer_size)
{
    std::cout << "Buffer size: " << buffer_size << " samples. \t\t Fixed size: " << results.fixed.count();
    std::cout << " ns, \t\t Fixed + switch: " << results.semi.count() << " ns. \t\t Variable: " << results.variable.count() << "ns\n";
}

template <void (*fixed_size_function)(float*, int*), int buffer_size>
Results run_test_int2float()
{
    Results results;
    std::array <std::array<float, buffer_size * RASPA_N_TOTAL_CHANNELS>, SUB_ITERATIONS> float_buffers;
    std::array <std::array<int, buffer_size * RASPA_N_TOTAL_CHANNELS>, SUB_ITERATIONS> int_buffers;
    auto fixed_timing = std::chrono::nanoseconds(0);
    auto semi_fixed_timing = std::chrono::nanoseconds(0);
    auto var_timing = std::chrono::nanoseconds(0);

    for (int i = 0; i < ITERATIONS; ++i)
    {
        for (auto& buffer : int_buffers)
        {
            buffer.fill(std::rand());
        }
        std::atomic_thread_fence(std::memory_order_acquire);

        /* Calling the fixed size function directly */
        auto start_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i<SUB_ITERATIONS; ++i)
        {
            fixed_size_function(float_buffers[i].data(), int_buffers[i].data());
        }
        auto stop_time = std::chrono::high_resolution_clock::now();
        fixed_timing += std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time);

        /* Calling the fixed size function through a switch */
        start_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i<SUB_ITERATIONS; ++i)
        {
            int2float(float_buffers[i].data(), int_buffers[i].data(), buffer_size);
        }
        stop_time = std::chrono::high_resolution_clock::now();
        semi_fixed_timing += std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time);

        /* Calling the general, variable size function */
        start_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i<SUB_ITERATIONS; ++i)
        {
            int2float_var(float_buffers[i].data(), int_buffers[i].data(), buffer_size);
        }
        stop_time = std::chrono::high_resolution_clock::now();
        var_timing += std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time);
    }
    results.fixed = fixed_timing / (ITERATIONS * SUB_ITERATIONS);
    results.semi = semi_fixed_timing / (ITERATIONS * SUB_ITERATIONS);
    results.variable = var_timing / (ITERATIONS * SUB_ITERATIONS);
    return results;
}

template <void (*fixed_size_function)(int*, float*), int buffer_size>
Results run_test_float2int()
{
    Results results;
    std::array <std::array<float, buffer_size * RASPA_N_TOTAL_CHANNELS>, SUB_ITERATIONS> float_buffers;
    std::array <std::array<int, buffer_size * RASPA_N_TOTAL_CHANNELS>, SUB_ITERATIONS> int_buffers;
    auto fixed_timing = std::chrono::nanoseconds(0);
    auto semi_fixed_timing = std::chrono::nanoseconds(0);
    auto var_timing = std::chrono::nanoseconds(0);

    for (int i = 0; i < ITERATIONS; ++i)
    {
        for (auto& buffer : float_buffers)
        {
            fill_buffer(buffer, 1.2f);
        }
        std::atomic_thread_fence(std::memory_order_acquire);

        /* Calling the fixed size function directly */
        auto start_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i<SUB_ITERATIONS; ++i)
        {
            fixed_size_function(int_buffers[i].data(), float_buffers[i].data());
        }
        auto stop_time = std::chrono::high_resolution_clock::now();
        fixed_timing += std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time);

        /* Calling the fixed size function through a switch */
        start_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i<SUB_ITERATIONS; ++i)
        {
            float2int(int_buffers[i].data(), float_buffers[i].data(), buffer_size);
        }
        stop_time = std::chrono::high_resolution_clock::now();
        semi_fixed_timing += std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time);

        /* Calling the general, variable size function */
        start_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i<SUB_ITERATIONS; ++i)
        {
            float2int_var(int_buffers[i].data(), float_buffers[i].data(), buffer_size);
        }
        stop_time = std::chrono::high_resolution_clock::now();
        var_timing += std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time);
    }
    results.fixed = fixed_timing / (ITERATIONS * SUB_ITERATIONS);
    results.semi = semi_fixed_timing / (ITERATIONS * SUB_ITERATIONS);
    results.variable = var_timing / (ITERATIONS * SUB_ITERATIONS);
    return results;
}

int main()
{
    std::cout << "Int to float conversion results: " << std::endl;
    print_results(run_test_int2float<int2float_b8, 8>(), 8);
    print_results(run_test_int2float<int2float_b16, 16>(), 16);
    print_results(run_test_int2float<int2float_b32, 32>(), 32);
    print_results(run_test_int2float<int2float_b64, 64>(), 64);
    print_results(run_test_int2float<int2float_b128, 128>(), 128);
    print_results(run_test_int2float<int2float_b256, 256>(), 256);
    print_results(run_test_int2float<int2float_b512, 512>(), 512);

    std::cout <<"\nFloat to int conversion results: " << std::endl;
    print_results(run_test_float2int<float2int_b8, 8>(), 8);
    print_results(run_test_float2int<float2int_b16, 16>(), 16);
    print_results(run_test_float2int<float2int_b32, 32>(), 32);
    print_results(run_test_float2int<float2int_b64, 64>(), 64);
    print_results(run_test_float2int<float2int_b128, 128>(), 128);
    print_results(run_test_float2int<float2int_b256, 256>(), 256);
    print_results(run_test_float2int<float2int_b512, 512>(), 512);
    return 0;
}
