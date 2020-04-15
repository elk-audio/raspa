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
 * @brief C API for low-level access to RTDM Audio device from userspace.
 * @copyright 2017-2020 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef RASPA_H_
#define RASPA_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// version definition
#define RASPA_VERSION_MAJ 0
#define RASPA_VERSION_MIN 1

/**
 * @brief Convert error codes to human readable strings.
 *
 * @param code Error code (negative number).
 */
const char* raspa_get_error_msg(int code);

/**
 * @brief Debug flag, signal debugger if a modeswith is detected.
 */
#define RASPA_DEBUG_SIGNAL_ON_MODE_SW   (1<<0)

typedef int64_t RaspaMicroSec;

/**
 * @brief Audio processing callback type
 *
 * @param input Audio input buffers in contiguous, non-interleaved format
 * @param output Audio output buffer in contiguous, non-interleaved format
 * @param data Opaque pointer to user-provided data given during callback registration
 */
typedef void (*RaspaProcessCallback)(float* input, float* output, void* data);

/**
 * @brief Initialization function, setting up Xenomai and locking memory for the
 *        process. Must be called before any other raspa calls.
 * @return 0 in case of success, linux error code otherwise
 */
int raspa_init();

/**
 * @brief Open device and check configuration with driver & audio controller
 *
 * @param buffer_size Number of frames in buffers processed at each interrupt
 * @param process_callback Pointer to user processing callback
 * @param user_data Opaque pointer of generic user data passed to callback during process
 * @param debug_flags Bitwise combination of debug flags to use
 *
 * @return 0 in case of success, negative value otherwise. raspa_get_error_msg()
 *         can be used to get a human readable string for the returned error code.
 */
int raspa_open(int buffer_size,
               RaspaProcessCallback process_callback,
               void* user_data, unsigned int debug_flags);

/**
 * @brief Get the sampling rate of driver. Should be called after raspa_open().
 *
 * @return float The sampling rate of the rt audio driver if raspa_open() is
 *               successful, 0.0 if not.
 */
float raspa_get_sampling_rate();

/**
 * @brief Get the number of input channels of the underlying codec. Should be
 *        called after raspa_open()
 * @return The number of input channels if raspa_open() is successful, 0 if not
 */
int raspa_get_num_input_channels();

/**
 * @brief Get the number of output channels of the underlying codec. Should be
 *        called after raspa_open
 * @return The number of output channels if raspa_open() is successful, 0 if not
 */
int raspa_get_num_output_channels();

/**
 * @brief Starts the real-time Xenomai task to perform audio processing
 *
 * @return 0 in case of success, negative value otherwise. raspa_get_error_msg()
 *         can be used to get a human readable string for the returned error code.
 */
int raspa_start_realtime();

/**
 * @brief Return the current time. Intended to be called from a Real-time task.
 *
 * @return The timestamp at the start of the current interrupt in microseconds,
 *         0 on failure to get time.
 *
 * @note   At the moment this neither accounts for processing latency or
 *         process call jitter.
 */
RaspaMicroSec raspa_get_time();

/**
 * @brief Get the number of samples passed. Intended to be called from a Real-time task.
 * @return The total samplecount at the start of the current interrupt.
 */
int64_t raspa_get_samplecount();

/**
 * @brief Query the current latency
 * @return The delay in milliseconds, from the interrupt time to when the first sample
 *         of the buffers appears on the physical output in microseconds.
 */
RaspaMicroSec raspa_get_output_latency();

/**
 * @brief Stop real-time processing task and close device.
 *
 * @return 0 in case of success, negative value otherwise. raspa_get_error_msg()
 *         can be used to get a human readable string for the returned error code.
 */
int raspa_close();

/**
 * @brief Get the cv in gates values
 *
 * @return returns the cv gates in values packed in a uint32_t
 */
uint32_t raspa_get_gate_values();

/**
 * @brief set the cv gate out values
 *
 * @param cv_gates_out packed into a 32 bit uint
 */
void raspa_set_gate_values(uint32_t cv_gates_out);

#ifdef __cplusplus
}
#endif

#endif // RASPA_H_
