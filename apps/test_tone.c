/*
 * Copyright 2018-2021 Modern Ancient Instruments Networked AB, dba Elk
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
 * @brief Program to generate test tone on all output channnels
 * @copyright 2017-2021 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <getopt.h>
#include <math.h>

#include "raspa/raspa.h"

#define DEFAULT_NUM_FRAMES  64
#define DEFAULT_OUTPUT_FREQ 440.0f
#define MIN_OUTPUT_FREQ 20.0f
#define MAX_OUTPUT_FREQ 20000.0f
#define DEFAULT_OUTPUT_GAIN 0.7f
#define MIN_OUTPUT_GAIN 0.0f
#define MAX_OUTPUT_GAIN 1.0f

static int num_frames = DEFAULT_NUM_FRAMES;
static bool log_file_enabled = false;
static int num_output_chans = 0;
static float sampling_rate = 0.0f;
static float output_gain = DEFAULT_OUTPUT_GAIN;
static float output_freq = DEFAULT_OUTPUT_FREQ;
static int sample_counter = 0;
static bool stop_flag = false;

void sigint_handler(int __attribute__((unused)) sig)
{
    stop_flag = true;
}

void print_usage(char *argv[])
{
    printf("\nProgram that generates test tone on all output channels.\n\n");
    printf("Usage: \n\n");
    printf("%s [-b]\n", argv[0]);
    printf("Options:\n");
    printf("    -h               : Help for usage options.\n");
    printf("    -b <buffer size> : Specify the audio buffer size.\n"
           "                       Default is %d. Ideally should be a \n"
           "                       power of 2\n", DEFAULT_NUM_FRAMES);
    printf("    -f               : Set output frequency (%f to %f).\n"
           "                       Default is %f.\n", MIN_OUTPUT_FREQ, MAX_OUTPUT_FREQ, DEFAULT_OUTPUT_FREQ);
    printf("    -g               : Set output gain (%f to %f)\n"
           "                       Default is %f\n", MIN_OUTPUT_GAIN, MAX_OUTPUT_GAIN, DEFAULT_OUTPUT_GAIN);
    printf("    -l               : Enable logging to %s\n", RASPA_DEFAULT_RUN_LOG_FILE);
    printf("    - stop the program with SIGINT\n\n");
}

void process(float* input, float* output, __attribute__((unused)) void* data)
{
    for (int i = 0; i < num_frames; i++)
    {
        float output_sample = output_gain * sin(2 * M_PI * sample_counter *
                                         output_freq / sampling_rate);

        sample_counter++;
        if (sample_counter == (int) sampling_rate)
        {
            sample_counter = 0;
        }

        for (int chan = 0; chan < num_output_chans; chan++)
        {
            output[i + (chan * num_frames)] = output_sample;
        }
    }
}

int main(int argc, char *argv[])
{
    int res = 0;
    int option = 0;

    // Argument parsing
    while ((option = getopt(argc, argv, "hb:f:g:l")) != -1)
    {
        switch (option)
        {
        case 'h' :
            print_usage(argv);
            exit(-1);
            break;

        case 'b' :
            num_frames = atoi(optarg);
            break;

        case 'f' :
            output_freq = (float)atof(optarg);
            if ((output_freq < MIN_OUTPUT_FREQ) || (output_freq > MAX_OUTPUT_FREQ))
            {
                fprintf(stderr, "Error: output frequency %f out of range\n", output_freq);
                exit(-1);
            }
            break;

        case 'g' :
            output_gain = atof(optarg);
            if ((output_gain < MIN_OUTPUT_GAIN) || (output_gain > MAX_OUTPUT_GAIN))
            {
                fprintf(stderr, "Error: output gain %f out of range\n", output_gain);
                exit(-1);
            }
            break;

        case 'l' :
            log_file_enabled = true;
            break;

        default:
            print_usage(argv);
            exit(-1);
            break;
        }
    }

    res = raspa_init();
    if (res < 0)
    {
        printf("Error initializing RASPA:%s\n", strerror(-res));
        exit(res);
    }

    signal(SIGINT, sigint_handler);

    res = raspa_open(num_frames, &process, 0, log_file_enabled ? RASPA_DEBUG_ENABLE_RUN_LOG_TO_FILE : 0);
    if (res < 0)
    {
        fprintf(stderr, "Error opening device: %s\n", raspa_get_error_msg(-res));
        exit(res);
    }

    num_output_chans = raspa_get_num_output_channels();
    sampling_rate = raspa_get_sampling_rate();

    printf("Test tone audio process started.\n");
    printf("Output frequency = %fHz.\n", output_freq);
    printf("Output gain      = %f\n", output_gain);
    raspa_start_realtime();

    // Non-RT processing loop
    while (!stop_flag)
    {
        sleep(1);
    }
    printf("\nClosing audio process...\n");

    raspa_close();

    printf("Done.\n");
    return 0;
}
