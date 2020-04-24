#include <chrono>
#include <iostream>
#include <array>
#include <atomic>

#include "sample_conversion.h"
#include "sample_converter_generic.h"

constexpr int ITERATIONS   = 500;
constexpr int SUB_ITERATIONS = 200;

float float_rand(float min, float max)
{
    return (((std::rand()) * (max - min)) / static_cast<float>(RAND_MAX)) + min;
}

void fill_buffer(float* buffer, float peak_value, int buffer_size_in_samples)
{
    for(int sample = 0; sample < buffer_size_in_samples; sample++)
    {
        buffer[sample] = float_rand(-peak_value, peak_value);
    }
}

struct Results
{
    std::chrono::nanoseconds fixed;
    std::chrono::nanoseconds variable;
};

void print_results(Results results, int buffer_size, int num_chans)
{
    float speed_up = (float)results.variable.count()/(float)results.fixed.count();
    std::cout << "Buffer size: " << buffer_size << ". Num chans: " << num_chans;
    std::cout << "\t\t Templated " << results.fixed.count() << " ns. \t\t Variable: " << results.variable.count() << "ns";
    std::cout << "\t\t Speedup : " << speed_up << std::endl;
}

void run_test_int2float()
{
    Results results;

    float* float_buffers[SUB_ITERATIONS];
    int32_t* int_buffers[SUB_ITERATIONS];

    auto fixed_timing = std::chrono::nanoseconds(0);
    auto var_timing = std::chrono::nanoseconds(0);

    int num_chans = raspa::MIN_NUM_CHANNELS;

    while (raspa::get_next_num_channels(num_chans).first)
    {
        int buffer_size = raspa::MIN_BUFFER_SIZE;
        while (raspa::get_next_buffer_size(buffer_size).first)
        {
            int buffer_size_in_samples = num_chans * buffer_size;
            fixed_timing = std::chrono::nanoseconds(0);
            var_timing = std::chrono::nanoseconds(0);

            for(int buf = 0; buf < SUB_ITERATIONS; buf++)
            {
                int res = posix_memalign((void**)&float_buffers[buf], 16, buffer_size_in_samples * sizeof(float))
                          || posix_memalign((void**)&int_buffers[buf], 16, buffer_size_in_samples * sizeof(int32_t));
                if(res < 0)
                {
                    std::cout << "Failed to get memory\n";
                    return;
                }
            }

            auto sample_converter = raspa::get_sample_converter(RaspaCodecFormat::INT24_LJ, buffer_size, num_chans);
            raspa::SampleConverterGeneric sample_converter_generic(RaspaCodecFormat::INT24_LJ, buffer_size, num_chans);

            for (int iter = 0; iter < ITERATIONS; ++iter)
            {
                for (int buf = 0; buf < SUB_ITERATIONS; buf++)
                {
                    for(int sample = 0; sample < buffer_size_in_samples; sample++)
                    {
                        int_buffers[buf][sample] = std::rand();
                    }
                }

                std::atomic_thread_fence(std::memory_order_acquire);

                /* Calling the general, variable size function */
                auto start_time = std::chrono::high_resolution_clock::now();
                for (int buf = 0; buf<SUB_ITERATIONS; ++buf)
                {
                    sample_converter_generic.codec_format_to_float32n(float_buffers[buf], int_buffers[buf]);
                }
                auto stop_time = std::chrono::high_resolution_clock::now();
                var_timing += std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time);

                /* Calling the fixed size function directly */
                start_time = std::chrono::high_resolution_clock::now();
                for (int buf = 0; buf<SUB_ITERATIONS; ++buf)
                {
                    sample_converter->codec_format_to_float32n(float_buffers[buf], int_buffers[buf]);
                }
                stop_time = std::chrono::high_resolution_clock::now();
                fixed_timing += std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time);
            }

            results.fixed = fixed_timing / (ITERATIONS * SUB_ITERATIONS);
            results.variable = var_timing / (ITERATIONS * SUB_ITERATIONS);

            print_results(results, buffer_size, num_chans);

            for(int buf = 0; buf < SUB_ITERATIONS; buf++)
            {
                free(float_buffers[buf]);
                free(int_buffers[buf]);
            }

            buffer_size = raspa::get_next_buffer_size(buffer_size).second;
        }

        num_chans = raspa::get_next_num_channels(num_chans).second;
        std::cout << std::endl;
    }
}

void run_test_float2int()
{
    Results results;

    float* float_buffers[SUB_ITERATIONS];
    int32_t* int_buffers[SUB_ITERATIONS];

    auto fixed_timing = std::chrono::nanoseconds(0);
    auto var_timing = std::chrono::nanoseconds(0);

    int num_chans = raspa::MIN_NUM_CHANNELS;

    while (raspa::get_next_num_channels(num_chans).first)
    {
        int buffer_size = raspa::MIN_BUFFER_SIZE;
        while (raspa::get_next_buffer_size(buffer_size).first)
        {
            int buffer_size_in_samples = num_chans * buffer_size;
            fixed_timing = std::chrono::nanoseconds(0);
            var_timing = std::chrono::nanoseconds(0);

            for(int buf = 0; buf < SUB_ITERATIONS; buf++)
            {
                int res = posix_memalign((void**)&float_buffers[buf], 16, buffer_size_in_samples * sizeof(float))
                        || posix_memalign((void**)&int_buffers[buf], 16, buffer_size_in_samples * sizeof(int32_t));
                if(res < 0)
                {
                    std::cout << "Failed to get memory\n";
                    return;
                }
            }

            auto sample_converter = raspa::get_sample_converter(RaspaCodecFormat::INT24_LJ, buffer_size, num_chans);
            raspa::SampleConverterGeneric sample_converter_generic(RaspaCodecFormat::INT24_LJ, buffer_size, num_chans);

            for (int iter = 0; iter < ITERATIONS; ++iter)
            {
                for(int buf = 0; buf < SUB_ITERATIONS; buf++)
                {
                    fill_buffer(float_buffers[buf], 1.2f, buffer_size_in_samples);
                }

                std::atomic_thread_fence(std::memory_order_acquire);

                auto start_time = std::chrono::high_resolution_clock::now();
                for (int buf = 0; buf<SUB_ITERATIONS; ++buf)
                {
                    sample_converter_generic.float32n_to_codec_format(int_buffers[buf], float_buffers[buf]);
                }
                auto stop_time = std::chrono::high_resolution_clock::now();
                var_timing += std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time);

                start_time = std::chrono::high_resolution_clock::now();
                for (int buf = 0; buf<SUB_ITERATIONS; ++buf)
                {
                    sample_converter->float32n_to_codec_format(int_buffers[buf], float_buffers[buf]);
                }
                stop_time = std::chrono::high_resolution_clock::now();
                fixed_timing += std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time);
            }

            results.fixed = fixed_timing / (ITERATIONS * SUB_ITERATIONS);
            results.variable = var_timing / (ITERATIONS * SUB_ITERATIONS);

            print_results(results, buffer_size, num_chans);

            for(int buf = 0; buf < SUB_ITERATIONS; buf++)
            {
                free(float_buffers[buf]);
                free(int_buffers[buf]);
            }
            buffer_size = raspa::get_next_buffer_size(buffer_size).second;
        }

        num_chans = raspa::get_next_num_channels(num_chans).second;
        std::cout << std::endl;
    }
}

int main()
{
    std::cout << "##############################################################\n";
    std::cout << "Int to float conversion results" << std::endl;
    std::cout << "##############################################################\n\n";
    run_test_int2float();

    std::cout << "##############################################################\n";
    std::cout <<"Float to int conversion results"<< std::endl;
    std::cout << "##############################################################\n\n";
    run_test_float2int();
    return 0;
}
