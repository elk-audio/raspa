/*
 * Copyright 2024 Elk Audio AB, Stockholm
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
 * @brief RASPA latency measurement application
 * @copyright 2024 Elk Audio AB, Stockholm
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
#include <inttypes.h>

#include "raspa/raspa.h"

#define DEFAULT_NUM_FRAMES 64

#define PULSE_PERIOD 0.5
#define PULSE_DURATION 200E-6f

struct pulse_generator
{
    int period;
    int period_counter;
    int duration;
    int duration_counter;
};

struct delay_measurement
{
    int measured_value;
    int counter;
    int captured;
};

static int num_frames = DEFAULT_NUM_FRAMES;
static int invert_phase_enabled = 0;
static int write_inverted_input_enabled = 0;
static int log_file_enabled = 0;
static int stop_flag = 0;
static int num_input_chans = 0;
static int num_output_chans = 0;

static struct pulse_generator pulse;
static struct delay_measurement *measurements = NULL;

void sigint_handler(int __attribute__((unused)) sig)
{
    stop_flag = 1;
}

void print_usage(char *argv[])
{
    printf("Latency measurement program.\n\n");
    printf("This program will generate short pulses on all audio outputs and then\n"
           "try to detect them on all the inputs, measuring the round trip delay.\n\n");
    printf("Please take care when using analog signals because the pulse\n"
           "detection logic on the inputs is very simple\n"
           "since it requires at least one signal sample being greater than 0.5 in amplitude.\n\n");
    printf("Also polarity is important, so if your analog path is inverting you should\n"
           "enable the -p option.\n\n");
    printf("Usage: \n\n");
    printf("%s [-b]\n", argv[0]);
    printf("Options:\n");
    printf("    -h               : Help for usage options.\n");
    printf("    -b <buffer size> : Specify the audio buffer size. \n"
           "                       Default is %d. Ideally should be a power of 2.\n", DEFAULT_NUM_FRAMES);
    printf("    -p               : Invert output phase.\n");
    printf("    -w               : Write inverted input to output while detecting positive pulse.\n");
    printf("    -l               : Enable logging to %s.\n\n", RASPA_DEFAULT_RUN_LOG_FILE);
    printf("    Stop the program with SIGINT\n\n");
    printf("Typical usage:\n\n");
    printf("    Digital loopback\n");
    printf("        1. Make a digital loopback on the PCM data lines (provided that they are in the same format).\n");
    printf("        2. Put an oscilloscope probe on the PCM output line.\n");
    printf("        3. Run the program without the -w option and read the measured latency from the console.\n"
           "           You should see the PCM output data on the oscilloscope moving every %.1f seconds for a duration of %.3f msec.\n", PULSE_PERIOD, 1E3 * PULSE_DURATION);
    printf("        4. Now run it again enabling the -w option and check the oscilloscope.\n"
           "           You should be able to detect the generated pattern (that is inverted input but only when trigger is detected)\n"
           "           and measure the I/O delay using the oscilloscope. It should be exactly equal to the one printed.\n\n");
    printf("    Analog loopback\n");
    printf("        1. Make an analog loopback in the Codec (output N -> input N). Usually the 1st output to the 1st input is enough.\n");
    printf("        2. Put an oscilloscope on one of the loopback channels.\n");
    printf("        3. Run the program without the -w option and read the measured latency from the console.\n"
           "           You should see the PCM output data on the oscilloscope moving every %.1f seconds for a duration of about %.3f msec.\n", PULSE_PERIOD, 1E3 * PULSE_DURATION);
    printf("        4. Now run it again enabling the -w option and check the oscilloscope.\n"
           "           You should be able to detect the generated pattern (that is inverted input but only when trigger is detected)\n"
           "           and measure the analog I/O delay using the oscilloscope.\n"
           "        Please note that with analog loopback you may need to invert the output phase with the -p option if the analog path is inverting.\n\n");
}

void print_delays(void)
{
    int channel_idx;
    float period_msec = 1000.0f / raspa_get_sampling_rate();

    printf("\n%" PRId64 " samples elapsed\n", raspa_get_samplecount());
    for (channel_idx = 0; channel_idx < num_input_chans; channel_idx++)
    {
        if (measurements[channel_idx].measured_value >= 0)
        {
            printf("Channel %d: %d samples (%f msec)\n", channel_idx, measurements[channel_idx].measured_value, measurements[channel_idx].measured_value * period_msec);
        }
        else
        {
            printf("Channel %d: NOT DETECTED!\n", channel_idx);
        }
    }
}

void measure_reset(void)
{
    int channel_idx;

    for (channel_idx = 0; channel_idx < num_input_chans; channel_idx++)
    {
        measurements[channel_idx].measured_value = -1;
        measurements[channel_idx].counter = 0;
        measurements[channel_idx].captured = 0;
    }
}

int measure_run(int channel_idx, float value)
{
    if (!measurements[channel_idx].captured)
    {
        if (value > 0.5f)
        {
            measurements[channel_idx].measured_value = measurements[channel_idx].counter;
            measurements[channel_idx].captured = 1;
        }
        else
        {
            measurements[channel_idx].counter++;
        }
    }

    return (value > 0.5f) ? 1 : 0;
}

void pulse_run(void)
{
    int channel_idx;

    pulse.period_counter++;
    if (pulse.period_counter == pulse.period)
    {
        pulse.period_counter = 0;
        pulse.duration_counter = 1; // start pulse
        measure_reset();
    }
    else
    {
        if (pulse.duration_counter == pulse.duration)
        {
            pulse.duration_counter = 0;
        }
        else if (pulse.duration_counter)
        {
            pulse.duration_counter++;
        }
    }
}

int pulse_active(void)
{
    return (pulse.duration_counter > 0) ? 1 : 0;
}

void process(float* input, float* output, __attribute__((unused)) void* data)
{
    int frame;
    int channel_idx;

    for (frame = 0; frame < num_frames; frame++)
    {
        pulse_run();
        for (channel_idx = 0; channel_idx < num_output_chans; channel_idx++)
        {
            if (pulse_active())
            {
                output[frame + channel_idx*num_frames] = invert_phase_enabled ? -1.0f : 1.0f;
            }
            else
            {
                output[frame + channel_idx*num_frames] = 0;
            }
        }

        for (channel_idx = 0; channel_idx < num_input_chans; channel_idx++)
        {
            float input_value = input[frame + channel_idx*num_frames];
            if (measure_run(channel_idx, input_value) && (channel_idx < num_output_chans) && write_inverted_input_enabled)
            {
                float output_value = -input_value;
                output[frame + channel_idx*num_frames] += invert_phase_enabled ? -output_value : output_value;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    float sample_rate;
    int res = 0;
    int option = 0;
    RaspaProcessCallback raspa_callback = NULL;

    // Argument parsing
    while ((option = getopt(argc, argv,":hb:pwlm:")) != -1)
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

        case 'p' :
            invert_phase_enabled = 1;
            break;

        case 'w' :
            write_inverted_input_enabled = 1;
            break;

        case 'l' :
            log_file_enabled = 1;
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

    res = raspa_open(num_frames, process, 0, log_file_enabled ? RASPA_DEBUG_ENABLE_RUN_LOG_TO_FILE : 0);
    if (res < 0)
    {
        fprintf(stderr, "Error opening device: %s\n", raspa_get_error_msg(-res));
        exit(res);
    }

    num_input_chans = raspa_get_num_input_channels();
    num_output_chans = raspa_get_num_output_channels();

    sample_rate = raspa_get_sampling_rate();
    pulse.period = sample_rate * PULSE_PERIOD;
    pulse.duration = sample_rate * PULSE_DURATION;
    pulse.period_counter = 0;
    pulse.duration_counter = 0;

    measurements = calloc(num_input_chans, sizeof(struct delay_measurement));
    measure_reset();

    printf("Latency measure process started\n");
    raspa_start_realtime();

    // Non-RT processing loop
    while (stop_flag == 0)
    {
        sleep(1);
        print_delays();
    }
    printf("\nClosing audio process...\n");

    raspa_close();

    free(measurements);

    printf("Done.\n");
    return 0;
}