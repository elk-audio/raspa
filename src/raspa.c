/*
 * Copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk
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
 * raspa.c
 *
 * @brief User-space access to RTDM Audio Driver
 * @copyright 2017-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#include "raspa/raspa.h"
#include "driver_config.h"
#include "sample_conversion.h"
#include "common_defs.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <sched.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <rtdm/rtdm.h>
#include <xenomai/init.h>
#include <cobalt/pthread.h>
#include <cobalt/time.h>
#include <cobalt/sys/ioctl.h>

#pragma GCC diagnostic pop

/**
 * Derived constants, put here to avoid cluttering shared headers.
 */
#define RASPA_CLOSE_DELAY_US                 500000
#define RASPA_ENABLE_MODE_SW_BREAK_DELAY     10
#define RASPA_NUM_PAGES_KERNEL_MEM           20

//#define RASPA_DEBUG_PRINT

/**
 * RASPA ERROR codes macro definitions
 */
 #define RASPA_ERROR_CODES(X)\
    X(0,     RASPA_SUCCESS, "Raspa: No error")\
    X(16,    RASPA_EBUSY, "Raspa: rtdm device busy")\
    X(135,   RASPA_EVERSION, "Raspa: Version mismatch with driver")\
    X(137,   RASPA_ECHANSIZE, "Raspa: Channel size mismatch with driver")\
    X(138,   RASPA_EBUFFSIZE, "Raspa: Buffer size mismatch with driver")\
    X(139,   RASPA_EPARAM, "Raspa: error reading parameter ")\
    X(140,   RASPA_EFIRMWARE, "Raspa: Audio controller firmware version mismatch with driver ")\
    X(1,     RASPA_EPERM, "Raspa: Buffers memory is too big")\
    X(5,     RASPA_IOE, "Raspa: Non-rt task failed")\
    X(12,    RASPA_ENOMEM, "Raspa: mmap to kernel buffers failed")\
    X(62,    RASPA_ETIME, "Raspa: Audio Controller did not respond")\
    X(26,    RASPA_EDEBUG, "Raspa: Debug related error")\

#define ERROR_ENUM(ID, NAME, TEXT) NAME = ID,
#define ERROR_TEXT(ID, NAME, TEXT) case ID: return TEXT;

enum
{
    RASPA_ERROR_CODES(ERROR_ENUM)
};

/**
 * Initialization phases
 * Used for proper cleanup in case of failure.
 */

typedef enum
{
    RASPA_INITPHASE_START,
    RASPA_INITPHASE_DEVICE_OPENED,
    RASPA_INITPHASE_USER_BUFFERS_ALLOCATED,
    RASPA_INITPHASE_MMAP_INITIALIZED,
    RASPA_INITPHASE_TASK_STARTED,
    RASPA_N_INITPHASES
} RaspaInitPhase;

/**
 * Getopt variable
 */
extern int optind;

/**
 * Module variables
 */

static volatile struct __attribute__((__packed__))
{
    volatile int32_t* rx;
    volatile int32_t* tx;
    volatile uint32_t* cv_gate_out;
    volatile uint32_t* cv_gate_in;
} _kernel_buffers;

static struct
{
    float*  input;
    float*  output;
} _user_buffers;

static int _device_handle = -1;

static int _interrupts_counter;

static int _double_buf_idx = 0;

static int _stop_request_flag = 0;

static RaspaProcessCallback _user_callback = 0;

static void* _user_data = NULL;

static pthread_t _processing_task;

static int _break_on_mode_sw = 0;

static size_t KERNEL_BUFFER_MEM_SIZE = 0;

static RaspaInitPhase _init_phase = RASPA_INITPHASE_START;

static int _sampling_freq = 0;

static int _num_frames = 0;

static int _num_samples = 0;

#ifdef RASPA_DEBUG_PRINT
static FILE *raspa_debug_file;
#endif

/**
 * Internal functions prototype
 */

static int _cleanup();

static void* _rt_task_callback(void* data);

/**
 * Functions implementation
 */

int raspa_init()
{
    int res;
    // Fake command line arguments to pass to xenomai_init()
    // For some obscure reasons, xenomai_init() crashes
    // if argv is allocated here on the stack, so we malloc it
    // beforehand.
    int argc = 2;
    char** argv = (char**) malloc((argc+1) * sizeof(char*));
    for (int i = 0; i < argc; i++)
    {
        argv[i] = (char*) malloc(32 * sizeof(char));
    }
    argv[argc] = NULL;
    strcpy(argv[0], "raspa");

    // add cpu affinity argument to xenomai init setting it to all cores.
    strcpy(argv[1], "--cpu-affinity=");
    for (int i = 0; i < get_nprocs(); i++)
    {
        char arg_cpu_num[2];
        sprintf(arg_cpu_num, "%d", i);

        // add cpu number to the list
        strncat(argv[1], arg_cpu_num, 2);

        // add comma except for last cpu number
        if(i != get_nprocs()-1)
        {
            strncat(argv[1], ",", 2);
        }
    }

    optind = 1;

    xenomai_init(&argc, (char* const**) &argv);
    KERNEL_BUFFER_MEM_SIZE = RASPA_NUM_PAGES_KERNEL_MEM * getpagesize();

    for (int i = 0; i < argc; i++)
    {
        free(argv[i]);
    }
    free(argv);

    res = mlockall(MCL_CURRENT|MCL_FUTURE);
    return res;
}

static int _raspa_read_driver_parameters(char * parameter)
{
    int rtdm_file;
    const int PATH_LEN = 100;
    const int VAL_STR_LEN = 25;
    char path[PATH_LEN];
    char value[VAL_STR_LEN];

    snprintf(path, PATH_LEN, RASPA_MODULE_PARAMETERS_PATH"/%s", parameter);

    rtdm_file = open(path, O_RDONLY);

    if (rtdm_file < 0)
    {
        return -RASPA_EPARAM;
    }

    if (-1 == read(rtdm_file, value, VAL_STR_LEN))
    {
        return -RASPA_EPARAM;
    }

    close(rtdm_file);

    return atoi(value);
}

static int _raspa_check_driver_compatibility(int buffer_size)
{
    if (RASPA_N_TOTAL_CHANNELS != _raspa_read_driver_parameters("audio_channels"))
    {
        return -RASPA_ECHANSIZE;
    }
    else if (buffer_size != _raspa_read_driver_parameters("audio_buffer_size"))
    {
        return -RASPA_EBUFFSIZE;
    }
    else if ((RASPALIB__REQUIRED_DRIVER_VERSION_MAJ != _raspa_read_driver_parameters("audio_ver_maj"))
            || (RASPALIB__REQUIRED_DRIVER_VERSION_MIN != _raspa_read_driver_parameters("audio_ver_min")))
    {
        return -RASPA_EVERSION;
    }
    else
    {
        return 0;
    }
}

int raspa_open(int buffer_size,
               RaspaProcessCallback process_callback,
               void* user_data, unsigned int debug_flags)
{
    int res = 0;
    volatile void *mm_buffer;

    _init_phase = RASPA_INITPHASE_START;

    _sampling_freq = _raspa_read_driver_parameters("audio_sampling_rate");
    if(_sampling_freq < 0)
    {
        return _sampling_freq;
    }

    res = _raspa_check_driver_compatibility(buffer_size);
    if (res != 0)
    {
        return res;
    }

    // store current buffer size
    _num_frames = buffer_size;
    _num_samples = _num_frames * RASPA_N_TOTAL_CHANNELS;

    if (debug_flags & RASPA_DEBUG_SIGNAL_ON_MODE_SW)
    {
        _break_on_mode_sw = 1;
    }

    res = __cobalt_open(RASPA_DEVICE_NAME, O_RDWR);
    if (res < 0)
    {
        return res;
    }
    _device_handle = res;
    _init_phase = RASPA_INITPHASE_DEVICE_OPENED;

    mm_buffer = __cobalt_mmap(NULL, KERNEL_BUFFER_MEM_SIZE,
                                         PROT_READ|PROT_WRITE,
                                         MAP_SHARED, _device_handle, 0);
    if (mm_buffer == MAP_FAILED)
    {
        _cleanup();
        return -RASPA_ENOMEM;
    }

    // Clear the memory location
    _init_phase = RASPA_INITPHASE_MMAP_INITIALIZED;

    // Kernel code maps contiguously tx buffers after rx ones
    _kernel_buffers.rx = mm_buffer;
    _kernel_buffers.tx = mm_buffer + sizeof(uint32_t) * _num_samples * 2;
    _kernel_buffers.cv_gate_out = mm_buffer + sizeof(uint32_t) * _num_samples * 4;
    _kernel_buffers.cv_gate_in = _kernel_buffers.cv_gate_out + 1;

    // Align buffers at 16 byte boundary.
    res =  posix_memalign((void**) &_user_buffers.input, 16, _num_samples * sizeof(float))
        || posix_memalign((void**) &_user_buffers.output, 16, _num_samples * sizeof(float));
    if (res)
    {
        _cleanup();
        return res;
    }
    _init_phase = RASPA_INITPHASE_USER_BUFFERS_ALLOCATED;

    _user_callback = process_callback;
    _user_data = user_data;
    _interrupts_counter = 0;
    _double_buf_idx = 0;

    return 0;
}

float raspa_get_sampling_rate()
{
    return (float) _sampling_freq;
}

int raspa_get_num_input_channels()
{
    return RASPA_N_INPUT_CHANNELS;
}

int raspa_get_num_output_channels()
{
    return RASPA_N_OUTPUT_CHANNELS;
}

int raspa_start_realtime()
{
    int res = 0;

    // Initialize RT task
    struct sched_param rt_params = { .sched_priority = RASPA_PROCESSING_TASK_PRIO };
    pthread_attr_t task_attributes;
    __cobalt_pthread_attr_init(&task_attributes);

    pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
    pthread_attr_setschedparam(&task_attributes, &rt_params);

    // Force affinity on first thread
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    res = pthread_attr_setaffinity_np(&task_attributes, sizeof(cpu_set_t), &cpuset);
    if (res < 0)
    {
        _cleanup();
        return res;
    }

    // Create rt thread
    res = __cobalt_pthread_create(&_processing_task, &task_attributes, &_rt_task_callback, _user_data);
    if (res < 0)
    {
        _cleanup();
        return res;
    }

    _init_phase = RASPA_INITPHASE_TASK_STARTED;
    usleep(10000);

    // After Xenomai init + RT thread creation, all non-RT threads have the affinity restricted to one
    // single core. This reverts back to the default of using all cores
    CPU_ZERO(&cpuset);
    for (int i = 0; i < get_nprocs(); i++)
    {
        CPU_SET(i, &cpuset);
    }
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    res = __cobalt_ioctl(_device_handle, RASPA_PROC_START);

    return res;
}

static int _cleanup()
{
    int res = 0;

    // Fall-through here is intentional to unwind
    // allocated resources in reverse order
    switch (_init_phase)
    {
    case RASPA_INITPHASE_TASK_STARTED:
        res |= pthread_cancel(_processing_task);
        res |= __cobalt_pthread_join(_processing_task, NULL);
        _sampling_freq = 0;
        // fallthrough

    case RASPA_INITPHASE_USER_BUFFERS_ALLOCATED:
        free(_user_buffers.input);
        free(_user_buffers.output);
        // fallthrough

    case RASPA_INITPHASE_MMAP_INITIALIZED:
        res |= munmap(_kernel_buffers.rx, KERNEL_BUFFER_MEM_SIZE);
        // fallthrough

    case RASPA_INITPHASE_DEVICE_OPENED:
        res |= __cobalt_close(_device_handle);
        _init_phase = RASPA_INITPHASE_START;
        // fallthrough
    default:
        break;
    }

    return res;
}

RaspaMicroSec raspa_get_time()
{
    RaspaMicroSec time = 0;
    struct timespec tp;
    int res = __cobalt_clock_gettime(CLOCK_MONOTONIC, &tp);
    if (res == 0)
    {
        time = (RaspaMicroSec)tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
    }
    return time;
}

int64_t raspa_get_samplecount()
{
    // TODO - Think of how to handle overruns.
    return _interrupts_counter * _num_frames;
}

RaspaMicroSec raspa_get_output_latency()
{
    // TODO - really crude approximation
    if(_sampling_freq > 0)
    {
        return (_num_samples * 1000000) / _sampling_freq;
    }

    return 0;
}

int raspa_close()
{
    int res;
    _stop_request_flag = 1;
    // Wait sometime for periodic task to send mute command to device
    usleep(RASPA_CLOSE_DELAY_US);

    if ((res = __cobalt_ioctl(_device_handle, RASPA_PROC_STOP)) < 0)
    {
        _cleanup();
        return res;
    }

    return _cleanup();
}

static void* _rt_task_callback(void* data)
{
    int res, offset = 0;

    while (1)
    {

        if ((res = __cobalt_ioctl(_device_handle, RASPA_IRQ_WAIT)) < 0)
        {
            // TODO: think how to handle this. Error here means something *bad*,
            // so we might want to de-register the driver, cleanup and signal
            // user someway.
            break;
        }

        // buffers ping-pong
        _double_buf_idx = res;
        offset = _double_buf_idx * _num_samples;

        if (_break_on_mode_sw && _interrupts_counter > RASPA_ENABLE_MODE_SW_BREAK_DELAY)
        {
            pthread_setmode_np(0, PTHREAD_WARNSW, NULL);
            _break_on_mode_sw = 0;
        }

        if (_stop_request_flag)
        {
            for (int i = 0; i < _num_samples; i++)
            {
                _kernel_buffers.tx[i + offset] = 0;
            }
        }
        else
        {
            RASPA_INT_TO_FLOAT(_user_buffers.input, &_kernel_buffers.rx[offset], _num_frames);
            _user_callback(_user_buffers.input, _user_buffers.output, data);
            RASPA_FLOAT_TO_INT(&_kernel_buffers.tx[offset], _user_buffers.output, _num_frames);
        }

        res = __cobalt_ioctl(_device_handle, RASPA_USERPROC_FINISHED, NULL);
#ifdef RASPA_DEBUG_PRINT
        if (res > 0)
        {
            __cobalt_printf("Audio driver: Under-run ! (No of under-runs: %d)\n",res);
        }
#endif
        _interrupts_counter++;
    }
    pthread_exit(NULL);
}

const char* raspa_get_error_msg(int code)
{
    switch (code)
    {
        RASPA_ERROR_CODES(ERROR_TEXT)
    }
    return "Raspa: Unknown error";
}

uint32_t raspa_get_gate_values()
{
    return _kernel_buffers.cv_gate_in[0];
}

void raspa_set_gate_values(uint32_t cv_gates_out)
{
    _kernel_buffers.cv_gate_out[0] = cv_gates_out;
}
