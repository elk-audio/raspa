/*
 * Copyright 2018-2020 Modern Ancient Instruments Networked AB, dba Elk
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
 * @brief RASPA loopback audio test program.
 * @copyright 2017-2020 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <getopt.h>

#include "raspa/raspa.h"

#define DEFAULT_NUM_FRAMES 64

static int num_frames = DEFAULT_NUM_FRAMES;
static int num_samples = 0;

void sigint_handler(int __attribute__((unused)) sig)
{
    raspa_close();
    printf("Device closed.\n");
    exit(0);
}

void print_usage(char *argv[])
{
    printf("Audio loopback program.\n\n");
    printf("Usage: \n\n");
    printf("%s [-b]\n", argv[0]);
    printf("Options:\n");
    printf("    -h               : Help for usage options.\n");
    printf("    -b <buffer size> : Specify the audio buffer size. \n"
           "                              Default is %d. Ideally should be a \n"
           "                              power of 2\n", DEFAULT_NUM_FRAMES);
    printf("    - stop the program with SIGINT\n\n");
}

void process(float* input, float* output, __attribute__((unused)) void* data)
{
    int i;
    for (i = 0; i < num_samples; i++)
    {
        *output++ = *input++;
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
    while ((option = getopt(argc, argv,"hb:")) != -1)
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

    // Calculate total num samples
    if(raspa_get_num_input_channels() > raspa_get_num_output_channels())
    {
        num_samples = num_frames * raspa_get_num_input_channels();
    }
    else
    {
        num_samples = num_frames * raspa_get_num_output_channels();
    }

    printf("Audio process started.\n");
    raspa_start_realtime();

    // Non-RT processing loop
    while (1)
    {
        sleep(1);
    }

    return 0;
}
