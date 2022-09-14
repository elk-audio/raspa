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
 * @brief RASPA loopback audio test program.
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

#include "raspa/raspa.h"

#define DEFAULT_NUM_FRAMES 64

static int num_frames = DEFAULT_NUM_FRAMES;
static int stop_flag = 0;
static int num_input_chans = 0;
static int num_output_chans = 0;
static enum
{
    NORMAL_MODE = 0,
    STEREO_MIX_MODE,
    NUM_MODE,
} mode = NORMAL_MODE;

void sigint_handler(int __attribute__((unused)) sig)
{
    stop_flag = 1;
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
    printf("    -m <mode>        : Specify the loopback mode: \n"
           "                              0 - Normal 1:1 loopback (Default).\n"
           "                              1 - Stereo mix loopback\n");
    printf("    - stop the program with SIGINT\n\n");
}

void normal_process(float* input, float* output, __attribute__((unused)) void* data)
{
    int frame;
    int channel_idx;
    int num_channels;

    if (num_input_chans < num_output_chans)
    {
        num_channels = num_input_chans;
    }
    else
    {
        num_channels = num_output_chans;
    }

    for (frame = 0; frame < num_frames; frame++)
    {
        for (channel_idx = 0; channel_idx < num_channels; channel_idx++)
        {
            output[frame + channel_idx*num_frames] = input[frame + channel_idx*num_frames];
        }
    }
}

void stereo_mix_process(float* input, float* output, __attribute__((unused)) void* data)
{
    int frame;
    int stereo_idx;

    // NOTE: this loop assumes both num_input_chans and num_output_chans to be multiple of 2
    for (frame = 0; frame < num_frames; frame++)
    {
        float l = 0;
        float r = 0;

        // Treat input channels as stereo pairs and sum all stereo channels together
        for (stereo_idx = 0; stereo_idx < num_input_chans/2; stereo_idx++)
        {
            l += input[frame + (2*stereo_idx + 0)*num_frames];
            r += input[frame + (2*stereo_idx + 1)*num_frames];
        }

        // Treat output channels as stereo pairs and assign them the sum of stereo inputs
        for (stereo_idx = 0; stereo_idx < num_output_chans/2; stereo_idx++)
        {
            output[frame + (2*stereo_idx + 0)*num_frames] = l;
            output[frame + (2*stereo_idx + 1)*num_frames] = r;
        }
    }
}

int main(int argc, char *argv[])
{
    int res = 0;
    int option = 0;
    RaspaProcessCallback raspa_callback = NULL;

    // Argument parsing
    while ((option = getopt(argc, argv,"hb:m:")) != -1)
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

        case 'm' :
            mode = atoi(optarg);
            break;

        default:
            print_usage(argv);
            exit(-1);
            break;
        }
    }

    // Mode check and callback selection
    switch (mode)
    {
        case NORMAL_MODE:
            raspa_callback = normal_process;
            break;
        case STEREO_MIX_MODE:
            raspa_callback = stereo_mix_process;
            break;
        default:
            printf("Unsupported mode %d\n", mode);
            exit(-1);
            break;
    }

    printf("\nRunning with mode %d and with a buffer size of %d.\n", mode, num_frames);

    res = raspa_init();
    if (res < 0)
    {
        printf("Error initializing RASPA:%s\n", strerror(-res));
        exit(res);
    }

    signal(SIGINT, sigint_handler);

    res = raspa_open(num_frames, raspa_callback, 0, 0);
    if (res < 0)
    {
        fprintf(stderr, "Error opening device: %s\n", raspa_get_error_msg(-res));
        exit(res);
    }

    num_input_chans = raspa_get_num_input_channels();
    num_output_chans = raspa_get_num_output_channels();

    // Sanity check for the number of channels got by raspa
    switch (mode)
    {
    case NORMAL_MODE:
        if (num_input_chans != num_output_chans)
        {
            printf("Warning: since the number of input "
                    "and output channels is different "
                    "then not all the channels will be used.\n");
        }
        break;

    case STEREO_MIX_MODE:
        if ((num_input_chans % 2) || (num_output_chans % 2))
        {
            printf("Error: stereo mix mode requires an even number "
                   "of inputs and output channels.\n");
            raspa_close();
            exit(-1);
        }
        break;

    default:
        break;
    }

    printf("Loopback audio process started.\n");
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
