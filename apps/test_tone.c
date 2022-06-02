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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <getopt.h>
#include <math.h>

#include "raspa/raspa.h"

#define DEFAULT_NUM_FRAMES 64

static int num_frames = DEFAULT_NUM_FRAMES;
static int num_output_chans = 0;
static float sampling_rate = 0.0f;
const static float output_gain = 0.7f;
const static float output_freq = 440.0f;
static int sample_counter = 0;
static int stop_flag = 0;

void sigint_handler(int __attribute__((unused)) sig)
{
    stop_flag = 1;
}

void print_usage(char *argv[])
{
    printf("\nProgram that generates test tone on all output channels.\n\n");
    printf("Usage: \n\n");
    printf("%s [-b]\n", argv[0]);
    printf("Options:\n");
    printf("    -h               : Help for usage options.\n");
    printf("    -b <buffer size> : Specify the audio buffer size. \n"
           "                       Default is %d. Ideally should be a \n"
           "                       power of 2\n", DEFAULT_NUM_FRAMES);
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

        output[i + (2 * num_frames)] = output_sample;
        output[i + (3 * num_frames)] = output_sample;

        // for (int chan = 0; chan < num_output_chans; chan++)
        // {
        //     output[i + (chan * num_frames)] = output_sample;
        //     //output[i + ( 9 * num_frames)] = output_sample;
        // }
    }
}

int main(int argc, char *argv[])
{
    int res = 0;
    int option = 0;
    if (argc < 2)
    {
        printf("\nRunning default buffer size of %d\n", DEFAULT_NUM_FRAMES);
        printf("For help use %s [-h]\n", argv[0]);
    }

    // Argument parsing
    while ((option = getopt(argc, argv, "hb:")) != -1)
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

    res = raspa_open(num_frames, &process, 0, 0);
    if (res < 0)
    {
        fprintf(stderr, "Error opening device: %s\n", raspa_get_error_msg(-res));
        exit(res);
    }

    num_output_chans = raspa_get_num_output_channels();
    sampling_rate = raspa_get_sampling_rate();

    printf("Test tone audio process started.\n");
    raspa_start_realtime();

    // Non-RT processing loop
    while (stop_flag == 0)
    {
        sleep(1);
    }
    printf("\nClosing audio process...\n");

    raspa_close();

    printf("Done.\n");
    return 0;
}
