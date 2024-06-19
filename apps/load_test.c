/*
 * Copyright 2022-2025 Modern Ancient Instruments Networked AB, dba Elk
 * RASPA is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * RASPA is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RASPA.
 * If not, see http://www.gnu.org/licenses/ .
 */

/**
 * @brief RASPA stress test program.
 * @copyright 2022-2025 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <getopt.h>

#include "raspa/raspa.h"

// default parameter values
#define DEFAULT_CPU                     -1
#define DEFAULT_NUM_FRAMES              64
#define DEFAULT_INPUT_CHANNEL           0
#define DEFAULT_OUTPUT_CHANNEL          0
#define DEFAULT_BIQUAD_NUM              128
#define DEFAULT_DELAY_LINE_NUM          128
#define DEFAULT_DELAY_LINE_LEN          (256 * 1024)
#define DEFAULT_DELAY_LINE_STEP         (32768 / 4)
#define DEFAULT_DELAY_LINE_TAPS         1
#define DEFAULT_DELAY_LINE_TAP_DELAY    8

#define MAX_DELAY_LINE_TAPS             64
#define MEM_ALLOC_ALIGN                 (1 * 1024 * 1024)

static int cpu = DEFAULT_CPU;
static bool stop_flag = false;
static int num_input_chans = 0;
static int num_output_chans = 0;
static int num_frames = DEFAULT_NUM_FRAMES;
static bool log_file_enabled = false;
static int input_channel = DEFAULT_INPUT_CHANNEL;
static int output_channel = DEFAULT_OUTPUT_CHANNEL;
static int num_biquad = DEFAULT_BIQUAD_NUM;
static int num_delay = DEFAULT_DELAY_LINE_NUM;
static int delay_line_len = DEFAULT_DELAY_LINE_LEN;
static int delay_line_step = DEFAULT_DELAY_LINE_STEP;
static int num_taps = DEFAULT_DELAY_LINE_TAPS;
static int tap_delay = DEFAULT_DELAY_LINE_TAP_DELAY;

struct BiquadCoeffs
{
    float a[2];
    float b[3];
    float gain;
};

struct BiquadData
{
    float z1;
    float z2;
};

struct DelayCoeffs
{
    float gain;
};

struct DelayData
{
    float *mem;
    size_t w_pos;
    size_t r_pos[MAX_DELAY_LINE_TAPS];
};

static struct
{
    struct BiquadCoeffs* biquad;
    struct DelayCoeffs* delay;
} k_mem;

static struct
{
    struct BiquadData* biquad;
    struct DelayData* delay;
} d_mem;

void sigint_handler(int __attribute__((unused)) sig)
{
    stop_flag = true;
}

void print_usage(char *argv[])
{
    printf("CPU load test program using biquads and delay lines.\n");
    printf("In order for this application to operate in the same conditions on all platforms\n"
           "the processing uses the first input and the first output buffers only.\n\n");
    printf("Here how processing works\n\n");
    printf("           |- biquad 1 -|\n");
    printf("           |- biquad 2 -|\n");
    printf("           |  ........  |\n");
    printf("           |- biquad f -|\n");
    printf("input 1 >--|            |--> output 1\n");
    printf("           |- delay  1 -|\n");
    printf("           |- delay  2 -|\n");
    printf("           |  ........  |\n");
    printf("           |- delay  d -|\n\n");
    printf("Usage: \n\n");
    printf("%s OPTIONS\n\n", argv[0]);
    printf("Options:\n");
    printf("    -h                    : Help for usage options.\n");
    printf("    -c <cpu>              : Specify the CPU to be used.\n"
           "                            Default is selected by raspa.\n");
    printf("    -b <buffer size>      : Specify the audio buffer size.\n"
           "                            Default is %d. Ideally should be a\n"
           "                            power of 2.\n",
                                        DEFAULT_NUM_FRAMES);
    printf("    -l                    : Enable logging to %s\n", RASPA_DEFAULT_RUN_LOG_FILE);
    printf("    -i <input_channel>    : Specify the input channel index.\n"
           "                            0 is the 1st channel.\n"
           "                            Default is %d.\n",
                                        DEFAULT_INPUT_CHANNEL);
    printf("    -o <output_channel>   : Specify the output channel index.\n"
           "                            0 is the 1st channel.\n"
           "                            Default is %d.\n",
                                        DEFAULT_INPUT_CHANNEL);
    printf("    -f <num_biquad>       : Specify the number of biquad filters.\n"
           "                            Default is %d.\n",
                                        DEFAULT_BIQUAD_NUM);
    printf("    -d <num_delay>        : Specify the number of delay lines.\n"
           "                            Default is %d.\n",
                                        DEFAULT_DELAY_LINE_NUM);
    printf("    -s <delay_line_size>  : Specify the length in samples of each\n"
           "                            delay line. Default is %d.\n",
                                        DEFAULT_DELAY_LINE_LEN);
    printf("    -x <delay_step>       : Specify the delay step in samples between.\n"
           "                            consecutive delay lines. First delay line\n"
           "                            has a delay of 0 samples. Default is %d.\n",
                                        DEFAULT_DELAY_LINE_STEP);
    printf("    -t <num_taps>         : Specify the number of delay line taps.\n"
           "                            Default is %d.\n"
           "                            Maximum is %d.\n",
                                        DEFAULT_DELAY_LINE_TAPS, MAX_DELAY_LINE_TAPS);
    printf("    -y <tap_delay>        : Specify the delay between consecutive taps.\n"
           "                            Default is %d.\n",
                                        DEFAULT_DELAY_LINE_TAP_DELAY);
    printf("    - stop the program with SIGINT\n\n");
}

void *alloc_mem(size_t num, size_t item_size)
{
    void *ptr;

    int res = posix_memalign(&ptr, MEM_ALLOC_ALIGN, num * item_size);
    if (res != 0)
    {
        ptr = NULL;
    }
    if (ptr != NULL)
    {
        res = mlock(ptr, num * item_size);
        if (res != 0)
        {
            free(ptr);
            ptr = NULL;
        }
        else
        {
            memset(ptr, 0, num * item_size);
        }
    }
    return ptr;
}

void free_mem(void)
{
    if (k_mem.delay)
    {
        free(k_mem.delay);
    }
    if (k_mem.biquad)
    {
        free(k_mem.biquad);
    }

    if (d_mem.delay)
    {
        for (int i = 0; i < num_delay; i++)
        {
            if (d_mem.delay[i].mem)
            {
                free(d_mem.delay[i].mem);
            }
        }
        free(d_mem.delay);
    }
    if (d_mem.biquad)
    {
        free(d_mem.biquad);
    }
}

void check_alloc(void *ptr)
{
    if (ptr == NULL)
    {
        free_mem();
        printf("Memory allocation error!\n");
        exit(-1);
    }
}

void biquad_init(void)
{
    if (num_biquad > 0)
    {
        k_mem.biquad = alloc_mem(num_biquad, sizeof(struct BiquadCoeffs));
        check_alloc(k_mem.biquad);

        d_mem.biquad = alloc_mem(num_biquad, sizeof(struct BiquadData));
        check_alloc(d_mem.biquad);

        for (int i = 0; i < num_biquad; i++)
        {
            k_mem.biquad[i].gain = 0.5f / num_biquad;
            k_mem.biquad[i].a[0] = 1.81531792f;
            k_mem.biquad[i].a[1] = -0.83098222f;
            k_mem.biquad[i].b[0] = 0.00391608f;
            k_mem.biquad[i].b[1] = 0.00783215f;
            k_mem.biquad[i].b[2] = 0.00391608f;
        }
    }
}

void delay_init(void)
{
    if (num_delay > 0)
    {
        k_mem.delay = alloc_mem(num_delay, sizeof(struct DelayCoeffs));
        check_alloc(k_mem.delay);

        d_mem.delay = alloc_mem(num_delay, sizeof(struct DelayData));
        check_alloc(d_mem.delay);

        for (int i = 0; i < num_delay; i++)
        {
            k_mem.delay[i].gain = 0.5f / num_delay / num_taps;
            d_mem.delay[i].w_pos = 0;
            for (int tap = 0; tap < num_taps; tap++)
            {
                int r_pos = -(i * delay_line_step + tap * tap_delay);
                while (r_pos < 0)
                {
                    r_pos += delay_line_len;
                }
                d_mem.delay[i].r_pos[tap] = r_pos;
            }
            d_mem.delay[i].mem = alloc_mem(delay_line_len, sizeof(float));
            check_alloc(d_mem.delay[i].mem);
        }
    }
}

void process(float* input, float* output, __attribute__((unused)) void* data)
{
    float x, y, z0;
    const float *selected_input = input + input_channel * num_frames;
    float *selected_output = output + output_channel * num_frames;

    // clear output buffer
    memset(selected_output, 0, num_frames * sizeof(float));

    // run biquads in parallel using the same input buffer and
    // accumulate on the same output buffer
    for (int biquad = 0; biquad < num_biquad; biquad++)
    {
        struct BiquadCoeffs *k = &k_mem.biquad[biquad];
        struct BiquadData *d = &d_mem.biquad[biquad];

        for (int i = 0; i < num_frames; i++)
        {
            x = selected_input[i] * k->gain; // input scaling

            // biquad
            z0 = x + k->a[0] * d->z1 + k->a[1] * d->z2;
            y  =  k->b[0] * z0 +
                  k->b[1] * d->z1 +
                  k->b[2] * d->z2;
            d->z2 = d->z1;
            d->z1 = z0;

            selected_output[i] += y; // accumulate on output
        }
    }

    // run delay lines in parallel using the same input buffer and
    // accumulate on the same output buffer
    for (int delay_line = 0; delay_line < num_delay; delay_line++)
    {
        struct DelayCoeffs *k = &k_mem.delay[delay_line];
        struct DelayData *d = &d_mem.delay[delay_line];

        for (int i = 0; i < num_frames; i++)
        {
            x = selected_input[i] * k->gain; // input scaling

            // delay line write and write index update
            d->mem[d->w_pos++] = x;
            d->w_pos %= delay_line_len;

            // delay line read (taps done inside this loop to stress more the cache!)
            for (int tap = 0; tap < num_taps; tap++)
            {
                y = d->mem[d->r_pos[tap]++];        // delay line read
                d->r_pos[tap] %= delay_line_len;    // tap read index update
                selected_output[i] += y;            // accumulate on output
            }
        }
    }
}

int main(int argc, char *argv[])
{
    int option = 0;
    int res;

    k_mem.biquad = NULL;
    k_mem.delay  = NULL;
    d_mem.biquad = NULL;
    d_mem.delay  = NULL;

    while ((option = getopt(argc, argv,"hc:b:li:o:f:d:s:x:t:y:")) != -1)
    {
        switch (option)
        {
        case 'h' :
            print_usage(argv);
            exit(-1);
            break;

        case 'c' :
            cpu = atoi(optarg);
            break;

        case 'b' :
            num_frames = atoi(optarg);
            break;

        case 'l' :
            log_file_enabled = true;
            break;

        case 'i' :
            input_channel = atoi(optarg);
            break;

        case 'o' :
            output_channel = atoi(optarg);
            break;

        case 'f' :
            num_biquad = atoi(optarg);
            break;

        case 'd' :
            num_delay = atoi(optarg);
            break;

        case 's' :
            delay_line_len = atoi(optarg);
            break;

        case 'x' :
            delay_line_step = atoi(optarg);
            break;

        case 't' :
            num_taps = atoi(optarg);
            if (num_taps > MAX_DELAY_LINE_TAPS)
            {
                printf("number of taps greater than %d!\n", MAX_DELAY_LINE_TAPS);
                exit(-1);
            }
            break;

        case 'y' :
            tap_delay = atoi(optarg);
            break;

        default:
            print_usage(argv);
            exit(-1);
            break;
        }
    }

    biquad_init();
    delay_init();

    signal(SIGINT, sigint_handler);

    res = raspa_init();
    if (res < 0)
    {
        printf("Error initializing RASPA in realtime mode:%s\n", strerror(-res));
        free_mem();
        exit(res);
    }

    if (cpu >= 0)
    {
        raspa_set_cpu_affinity(cpu);
    }

    res = raspa_open(num_frames, process, 0, log_file_enabled ? RASPA_DEBUG_ENABLE_RUN_LOG_TO_FILE : 0);
    if (res < 0)
    {
        fprintf(stderr, "Error opening device: %s\n", raspa_get_error_msg(-res));
        free_mem();
        exit(res);
    }

    num_input_chans = raspa_get_num_input_channels();
    num_output_chans = raspa_get_num_output_channels();

    if ((num_input_chans < 1) || (num_output_chans < 1))
    {
        printf("Error: required at least 1 input and 1 output channel.\n");
        raspa_close();
        free_mem();
        exit(-1);
    }

    if (input_channel >= num_input_chans)
    {
        printf("Error: input channel is not available!\n");
        raspa_close();
        free_mem();
        exit(-1);
    }

    if (output_channel >= num_output_chans)
    {
        printf("Error: output channel is not available!\n");
        raspa_close();
        free_mem();
        exit(-1);
    }

    printf("Load test audio process started.\n");
    raspa_start_realtime();

    // Non-RT processing loop
    while (!stop_flag)
    {
        sleep(1);
    }
    printf("\nClosing audio process...\n");

    raspa_close();

    free_mem();

    printf("Done.\n");
    return 0;
}