/*
 * Copyright 2018-2021 Modern Ancient Instruments Networked AB, dba Elk
 * RASPA is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * RASPA is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * RASPA. If not, see http://www.gnu.org/licenses/ .
 */

/**
 * @brief Definition of class RaspaPimpl, which abstracts low-level access
 *        to a RTDM Audio device from userspace in scenarios where the driver
 *        directly interfaces with the CODECS. Provides access to RT driver
 *        through a typical callback registration service. This class provides a
 *        private implementation of the api found in raspa.h
 * @copyright 2017-2021 Modern Ancient Instruments Networked AB, dba Elk,
 * Stockholm
 */
#ifndef RASPA_RASPA_PIMPL_H
#define RASPA_RASPA_PIMPL_H

#include <sched.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <error.h>
#include <errno.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"

#ifdef RASPA_WITH_EVL
    #include <unistd.h>
    #include <evl/evl.h>
    #include <sys/ioctl.h>
    #include <evl/syscall.h>
    #include <evl/clock.h>
#else
    #include <cobalt/pthread.h>
    #include <cobalt/sys/ioctl.h>
    #include <cobalt/time.h>
    #include <rtdm/rtdm.h>
    #include <xenomai/init.h>
#endif

#pragma GCC diagnostic pop

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <algorithm>

#include "audio_control_protocol/audio_control_protocol.h"
#include "audio_control_protocol/audio_packet_helper.h"
#include "audio_control_protocol/device_control_protocol.h"
#include "driver_config.h"
#include "raspa/raspa.h"
#include "raspa_delay_error_filter.h"
#include "raspa_error_codes.h"
#include "raspa_gpio_com.h"
#include "sample_conversion.h"

#ifdef RASPA_DEBUG_PRINT
    #include <stdio.h>
#endif

#ifdef RASPA_WITH_EVL
    #define __RASPA_IOCTL_RT(call)		oob_ ## call
    #define __RASPA(call)		        call
#else
    #define __RASPA_IOCTL_RT(call)		__cobalt_ ## call
    #define __RASPA(call)		        __cobalt_ ## call
#endif

/*
 * This variable is defined by xenomai init. It is used to index the number
 * of command line arguments passed to xenomai. Since these arguments are passed
 * manually, this variable is incremented in the init() function.
 */
extern int optind;

namespace raspa {

/**
 * @brief RtGpio is used to communicate with the driver for handling
 * gpios in real-time context
 *
 */
typedef struct rt_gpio {
    int num;
    int dir;
    int val;
} RtGpio;

// Delay in microseconds to wait for audio driver to close and stop its thread.
constexpr int CLOSE_DELAY_US = 500000;

// Delay in microseconds for a stop request to be processed
constexpr int STOP_REQUEST_DELAY_US = 10000;

constexpr int THREAD_CREATE_DELAY_US = 10000;

// Number of kernel memory pages raspa allocates
constexpr int NUM_PAGES_KERNEL_MEM = 20;

// Num of audio buffers.
constexpr int NUM_BUFFERS = 2;

// settling constant for the delay filter
constexpr int DELAY_FILTER_SETTLING_CONSTANT = 100;

// Down sampling rate for the delay filter
constexpr int DELAY_FILTER_DOWNSAMPLE_RATE = 16;

// SENSEI socket address
constexpr char SENSEI_SOCKET[] = "/tmp/sensei";

// manually passed "commandline args" to xenomai
constexpr char XENOMAI_ARG_APP_NAME[] = "raspa";
constexpr char XENOMAI_ARG_CPU_AFFINITY_DUAL_CORE[] = "--cpu-affinity=0,1";
constexpr char XENOMAI_ARG_CPU_AFFINITY_QUAD_CORE[] = "--cpu-affinity=0,1,2,3";

/**
 * @brief Entry point for the real time thread
 * @param data Contains pointer to an instance of RaspaPimpl
 * @return nullptr
 */
static void* raspa_pimpl_task_entry(void* data);

/**
 * @brief Interface to a audio rtdm driver that directly interfaces with a
 *        coded. It handles all low level access and is responsible for querying
 *        various audio parameters from the driver, starting and stopping of
 *        real time audio callbacks, interleaving/deinterleaving and conversion
 *        of audio samples. All public functions follow the same api as defined
 *        raspa.h
 */
class RaspaPimpl
{
public:
    RaspaPimpl() :
            _driver_buffer(nullptr),
            _driver_buffer_audio_in{nullptr, nullptr},
            _driver_buffer_audio_out{nullptr, nullptr},
            _driver_cv_in{nullptr},
            _driver_cv_out{nullptr},
            _tx_pkt{nullptr, nullptr},
            _rx_pkt{nullptr, nullptr},
            _kernel_buffer_mem_size(0),
            _user_audio_in{nullptr},
            _user_audio_out{nullptr},
            _user_gate_in(0),
            _user_gate_out(0),
            _device_handle(-1),
            _interrupts_counter(0),
            _stop_request_flag(false),
            _break_on_mode_sw(false),
            _sample_rate(0.0),
            _num_codec_chans(0),
            _num_input_chans(0),
            _num_output_chans(0),
            _buffer_size_in_frames(0),
            _buffer_size_in_samples(0),
            _codec_format(driver_conf::CodecFormat::INT24_LJ),
            _device_opened(false),
            _user_buffers_allocated(false),
            _mmap_initialized(false),
            _task_started(false),
            _user_data(nullptr),
            _user_callback(nullptr),
            _platform_type(driver_conf::PlatformType::NATIVE),
            _error_filter_process_count(0),
            _audio_packet_seq_num(0)
    {}

    ~RaspaPimpl()
    {
        _cleanup();
    }

    int init()
    {
#ifndef RASPA_WITH_EVL
        /*
         * Fake command line arguments to pass to xenomai_init(). For some
         * obscure reasons, xenomai_init() crashes if argv is allocated here on
         * the stack, so we alloc it beforehand.
         */
        int argc = 2;
        auto argv = new char*[argc + 1];
        for (int i = 0; i < argc; i++)
        {
            argv[i] = new char[32];
        }
        argv[argc] = nullptr;

        std::snprintf(argv[0],
                      sizeof(XENOMAI_ARG_APP_NAME),
                      XENOMAI_ARG_APP_NAME);

        // dual core
        if (get_nprocs() == 2)
        {
            std::snprintf(argv[1],
                          sizeof(XENOMAI_ARG_CPU_AFFINITY_DUAL_CORE),
                          XENOMAI_ARG_CPU_AFFINITY_DUAL_CORE);
        }
        // quad core
        else if (get_nprocs() == 4)
        {
            std::snprintf(argv[1],
                          sizeof(XENOMAI_ARG_CPU_AFFINITY_QUAD_CORE),
                          XENOMAI_ARG_CPU_AFFINITY_QUAD_CORE);
        }

        optind = 1;

        xenomai_init(&argc, (char* const**) &argv);

        for (int i = 0; i < argc; i++)
        {
            free(argv[i]);
        }
        free(argv);


        auto res = mlockall(MCL_CURRENT | MCL_FUTURE);
        if (res < 0)
        {
            _raspa_error_code.set_error_val(RASPA_EMLOCKALL, res);
            return -RASPA_EMLOCKALL;
        }
#endif
        _kernel_buffer_mem_size = NUM_PAGES_KERNEL_MEM * getpagesize();

        return RASPA_SUCCESS;
    }

    int open_device(int buffer_size,
             RaspaProcessCallback process_callback,
             void* user_data,
             unsigned int debug_flags)
    {
        // check if driver version is ok
        auto ver_check = driver_conf::check_driver_version();
        if (!ver_check.first)
        {
            // if unable to read parameter
            if (ver_check.second < 0)
            {
                _raspa_error_code.set_error_val(RASPA_EPARAM_VERSION,
                                            ver_check.second);
                return -RASPA_EPARAM_VERSION;
            }

            // version mismatch
            return -RASPA_EVERSION;
        }

        auto res = _get_audio_info_from_driver();
        if (res != RASPA_SUCCESS)
        {
            return res;
        }

        // check driver buffer size
        _buffer_size_in_frames = buffer_size;
        res = _validate_buffer_size();
        if (res != RASPA_SUCCESS)
        {
            return res;
        }

        if (debug_flags == 1 && RASPA_DEBUG_SIGNAL_ON_MODE_SW == 1)
        {
            _break_on_mode_sw = true;
        }

        res = _open_device();
        if (res < 0)
        {
            return res;
        }

        res = _get_driver_buffers();
        if (res < 0)
        {
            _cleanup();
            return res;
        }

        _init_driver_buffers();

        res = _init_user_buffers();
        if (res < 0)
        {
            _cleanup();
            return res;
        }

        _init_sample_converter();
        if (!_sample_converter)
        {
            return -RASPA_EBUFFER_SIZE_SC;
        }

        // Delay filter is needed for synchronization
        if (_platform_type == driver_conf::PlatformType::SYNC)
        {
            _init_delay_error_filter();
        }

        if (_platform_type != driver_conf::PlatformType::NATIVE)
        {
            res = _init_gpio_com();
            if (res != RASPA_SUCCESS)
            {
                return res;
            }
        }

        _user_data = user_data;
        _interrupts_counter = 0;
        _user_callback = process_callback;

        return RASPA_SUCCESS;
    }

    int start_realtime()
    {
        // Initialize RT task
        _task_started = false;
        struct sched_param rt_params = {
                            .sched_priority = RASPA_PROCESSING_TASK_PRIO};
        pthread_attr_t task_attributes;
        pthread_attr_init(&task_attributes);

        pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
        pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
        pthread_attr_setschedparam(&task_attributes, &rt_params);

        // Force affinity on first thread
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);
        auto res = pthread_attr_setaffinity_np(&task_attributes,
                                               sizeof(cpu_set_t),
                                               &cpuset);
        if (res < 0)
        {
            _cleanup();
            _raspa_error_code.set_error_val(RASPA_ETASK_AFFINITY, res);
            return -RASPA_ETASK_AFFINITY;
        }

        // Create rt thread
        res = __RASPA(pthread_create(&_processing_task,
                                      &task_attributes,
                                      &raspa_pimpl_task_entry,
                                      this));
        if (res < 0)
        {
            _cleanup();
            _raspa_error_code.set_error_val(RASPA_ETASK_CREATE, res);
            return -RASPA_ETASK_CREATE;
        }

        _task_started = true;
        usleep(THREAD_CREATE_DELAY_US);

        /* After Xenomai init + RT thread creation, all non-RT threads have the
         * affinity restricted to one single core. This reverts back to the
         * default of using all cores */
        CPU_ZERO(&cpuset);
        for (int i = 0; i < get_nprocs(); i++)
        {
            CPU_SET(i, &cpuset);
        }
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

        res = __RASPA(ioctl(_device_handle, RASPA_PROC_START));
        if (res < 0)
        {
            _raspa_error_code.set_error_val(RASPA_ETASK_START, res);
            return -RASPA_ETASK_START;
        }

        return RASPA_SUCCESS;
    }

    /**
     * @brief The main rt audio loop
     */
    void rt_loop()
    {
        switch (_platform_type)
        {
        case driver_conf::PlatformType::NATIVE:
            _rt_loop_native();
            break;

        case driver_conf::PlatformType::SYNC:
            _rt_loop_sync();
            break;

        case driver_conf::PlatformType::ASYNC:
            _rt_loop_async();
            break;
        }

        pthread_exit(nullptr);
    }

    float get_sampling_rate()
    {
        return _sample_rate;
    }

    int get_num_input_channels()
    {
        return _num_input_chans;
    }

    int get_num_output_channels()
    {
        return _num_output_chans;
    }

    const char* get_error_msg(int code)
    {
        return _raspa_error_code.get_error_text(code);
    }

    uint32_t get_gate_values()
    {
        return _user_gate_in;
    }

    void set_gate_values(uint32_t gate_out_val)
    {
        _user_gate_out = gate_out_val;
    }

    RaspaMicroSec get_time()
    {
        RaspaMicroSec time = 0;
        struct timespec tp;

#ifdef RASPA_WITH_EVL
        auto res = evl_read_clock(EVL_CLOCK_MONOTONIC, &tp);
#else
        auto res = __cobalt_clock_gettime(CLOCK_MONOTONIC, &tp);
#endif
        if (res == 0)
        {
            time = (RaspaMicroSec) tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
        }

        return time;
    }

    int64_t get_samplecount()
    {
        return _interrupts_counter * _buffer_size_in_frames;
    }

    RaspaMicroSec get_output_latency()
    {
        // TODO - really crude approximation
        if (_sample_rate > 0)
        {
            return (_buffer_size_in_samples * 1000000) / _sample_rate;
        }

        return 0;
    }

    int close_device()
    {
        _stop_request_flag = true;

        // Wait sometime for periodic task to send mute command to device
        usleep(STOP_REQUEST_DELAY_US);

        auto res = __RASPA(ioctl(_device_handle, RASPA_PROC_STOP));

        // Wait for driver to stop current transfers.
        usleep(CLOSE_DELAY_US);

        if (res < 0)
        {
            _cleanup();
            _raspa_error_code.set_error_val(RASPA_ETASK_STOP, res);
            return -RASPA_ETASK_STOP;
        }

        return _cleanup();
    }

    int request_out_gpio(int pin_num)
    {
        RtGpio rtgpio_requested;
        rtgpio_requested.num = pin_num;
        rtgpio_requested.val = 0;

        auto res = __RASPA(ioctl(_device_handle, RASPA_GPIO_GET_PIN,
                                    &rtgpio_requested));
        if (res)
        {
            return -RASPA_EGPIO_UNSUPPORTED;
        }
        res = __RASPA(ioctl(_device_handle, RASPA_GPIO_SET_DIR_OUT,
                                &rtgpio_requested));
        return 0;
    }

    int set_gpio(int pin_num, int val)
    {
        RtGpio rtgpio_requested;
        rtgpio_requested.num = pin_num;
        rtgpio_requested.val = val;

        auto res = __RASPA_IOCTL_RT(ioctl(_device_handle, RASPA_GPIO_SET_VAL,
                                    &rtgpio_requested));
        if (res)
        {
            return -RASPA_EGPIO_UNSUPPORTED;
        }
        return 0;
    }

    int free_gpio(int pin_num)
    {
        RtGpio rtgpio_requested;
        rtgpio_requested.num = pin_num;
        auto res = __RASPA(ioctl(_device_handle, RASPA_GPIO_RELEASE,
                                    &rtgpio_requested));
        if (res)
        {
            printf("RASPA_GPIO_RELEASE ret = %d\n", res);
            return -RASPA_EGPIO_UNSUPPORTED;
        }
        return 0;
    }

protected:
    /**
     * @brief Get the various info from the drivers parameter
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _get_audio_info_from_driver()
    {
        auto sample_rate = driver_conf::get_sample_rate();
        _num_input_chans = driver_conf::get_num_input_chan();
        _num_output_chans = driver_conf::get_num_output_chan();
        auto codec_format = driver_conf::get_codec_format();
        auto platform_type = driver_conf::get_platform_type();

        // sanity checks on the parameters
        if (sample_rate < 0)
        {
            _raspa_error_code.set_error_val(RASPA_EPARAM_SAMPLERATE,
                                            sample_rate);
            return -RASPA_EPARAM_SAMPLERATE;
        }
        else if (_num_input_chans < 0)
        {
            _raspa_error_code.set_error_val(RASPA_EPARAM_INPUTCHANS,
                                            _num_input_chans);
            return -RASPA_EPARAM_INPUTCHANS;
        }
        else if (_num_output_chans < 0)
        {
            _raspa_error_code.set_error_val(RASPA_EPARAM_OUTPUTCHANS,
                                            _num_output_chans);
            return -RASPA_EPARAM_OUTPUTCHANS;
        }
        else if (codec_format < 0)
        {
            _raspa_error_code.set_error_val(RASPA_EPARAM_CODEC_FORMAT,
                                            codec_format);
            return -RASPA_EPARAM_CODEC_FORMAT;
        }
        else if (platform_type < 0)
        {
            _raspa_error_code.set_error_val(RASPA_EPARAM_PLATFORM_TYPE,
                                            platform_type);
            return -RASPA_EPARAM_PLATFORM_TYPE;
        }

        _sample_rate = static_cast<float>(sample_rate);

        // set internal codec format
        if (codec_format < static_cast<int>(driver_conf::CodecFormat::
                                                                INT24_LJ) ||
            codec_format >= static_cast<int>(driver_conf::CodecFormat::
                                                                 NUM_CODEC_FORMATS))
        {
            _raspa_error_code.set_error_val(RASPA_ECODEC_FORMAT, codec_format);
            return -RASPA_ECODEC_FORMAT;
        }
        _codec_format = static_cast<driver_conf::CodecFormat>(codec_format);

        // set internal platform type
        if (platform_type < static_cast<int>(driver_conf::PlatformType::
                                                                 NATIVE) ||
            platform_type > static_cast<int>(driver_conf::PlatformType::ASYNC))
        {
            _raspa_error_code.set_error_val(RASPA_EPLATFORM_TYPE,
                                            platform_type);
            return -RASPA_EPARAM_PLATFORM_TYPE;
        }
        _platform_type = static_cast<driver_conf::PlatformType>(platform_type);

        // set number of codec channels
        _num_codec_chans = (_num_input_chans > _num_output_chans) ?
                                               _num_input_chans :
                                               _num_output_chans;

        return RASPA_SUCCESS;
    }

    /**
     * @brief Checks if a buffer size specified matches with that of the driver.
     *
     * @return int RASPA_SUCCESS upon success, negative raspa error code
     *         otherwise
     */
    int _validate_buffer_size()
    {
        auto driver_buffer_size = driver_conf::get_buffer_size();
        if (driver_buffer_size < 0)
        {
            _raspa_error_code.set_error_val(RASPA_EPARAM_BUFFER_SIZE,
                                            driver_buffer_size);
            return -RASPA_EPARAM_BUFFER_SIZE;
        }

        if (driver_buffer_size != _buffer_size_in_frames)
        {
            return -RASPA_EBUFFER_SIZE_MISMATCH;
        }

        return RASPA_SUCCESS;
    }

    /**
     * @brief Open the rtdm device.
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _open_device()
    {
        _device_opened = false;

        _device_handle = __RASPA(open(driver_conf::DEVICE_NAME, O_RDWR));

        if (_device_handle < 0)
        {
            // check if driver has been configured with invalid buffer size
            if (errno ==
                static_cast<int>(driver_conf::ErrorCode::INVALID_BUFFER_SIZE))
            {
                return -RASPA_EBUFFER_SIZE_INVALID;
            }

            // check if it is external micro-controller related issues
            if (_platform_type != driver_conf::PlatformType::NATIVE)
            {
                if (errno ==
                    static_cast<int>(driver_conf::ErrorCode::DEVICE_INACTIVE))
                {
                    return -RASPA_EDEVICE_INACTIVE;
                }
                else if (errno ==
                         static_cast<int>(driver_conf::ErrorCode::
                                                              INVALID_FIRMWARE_VER))
                {
                    return -RASPA_EDEVICE_FIRMWARE;
                }
            }

            // other errors
            _raspa_error_code.set_error_val(RASPA_EDEVICE_OPEN, _device_handle);
            return -RASPA_EDEVICE_OPEN;
        }

        _device_opened = true;
        return RASPA_SUCCESS;
    }

    /**
     * @brief Close the rtdm device
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _close_device()
    {
        if (_device_opened)
        {
            auto res = __RASPA(close(_device_handle));
            _device_opened = false;
            if (res < 0)
            {
                _raspa_error_code.set_error_val(RASPA_EDEVICE_CLOSE, res);
                return -RASPA_EDEVICE_CLOSE;
            }
        }

        return RASPA_SUCCESS;
    }

    /**
     * @brief Get audio buffers from the driver using mmap.
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _get_driver_buffers()
    {
        _mmap_initialized = false;

        auto buffer = __RASPA(mmap(NULL,
                                    _kernel_buffer_mem_size,
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED,
                                    _device_handle,
                                    0));
        _driver_buffer = static_cast<int32_t*>(buffer);
        if (_driver_buffer == MAP_FAILED)
        {
            _raspa_error_code.set_error_val(RASPA_ENOMEM, errno);
            return -RASPA_ENOMEM;
        }

        _mmap_initialized = true;
        return RASPA_SUCCESS;
    }

    /**
     * @brief Un maps the acquired audio buffers from the driver.
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _release_driver_buffers()
    {
        if (_mmap_initialized)
        {
            auto res = munmap(_driver_buffer, _kernel_buffer_mem_size);
            _mmap_initialized = false;
            if (res < 0)
            {
                _raspa_error_code.set_error_val(RASPA_EUNMAP, res);
                return -RASPA_EUNMAP;
            }
        }

        return RASPA_SUCCESS;
    }

    /**
     * @brief Initialize the input and output double buffers from the driver.
     *        The arrangement of the buffers are dependent on the platform
     *        type as those which use external micro controllers use the
     *        audio control protocol to communicate info to it. The arrangement
     *        of the driver buffer is as follows
     *
     *        For PlatformType::NATIVE:
     *        1. audio buffer in number 0
     *        2. audio buffer in number 1
     *        3. audio buffer out number 0
     *        4. audio buffer out number 1
     *
     *        For other platform types
     *        1. rx device control packet 0
     *        2. rx audio control packet number 0
     *        3. audio buffer in number 0
     *        4. rx device control packet 1
     *        5. rx audio control packet number 1
     *        6. audio buffer in number 1
     *        7. tx device control packet 0
     *        8. tx audio control packet number 0
     *        9. audio buffer out number 0
     *        10. tx device control packet 1
     *        11. tx audio control packet number 1
     *        12. audio buffer out number 1
     */
    void _init_driver_buffers()
    {
        _buffer_size_in_samples = _buffer_size_in_frames * _num_codec_chans;

        /* If raspa platform type is not native, then the driver buffers
         * also include space for audio control packet and device control packet.
         */
        if (_platform_type != driver_conf::PlatformType::NATIVE)
        {
            int32_t* ptr = _driver_buffer + DEVICE_CTRL_PKT_SIZE_WORDS;
            _rx_pkt[0] = reinterpret_cast<audio_ctrl::AudioCtrlPkt*> (ptr);

            ptr += AUDIO_CTRL_PKT_SIZE_WORDS;
            _driver_buffer_audio_in[0] = ptr;

            ptr += _buffer_size_in_samples + DEVICE_CTRL_PKT_SIZE_WORDS;
            _rx_pkt[1] = reinterpret_cast<audio_ctrl::AudioCtrlPkt*>(ptr);

            ptr += AUDIO_CTRL_PKT_SIZE_WORDS;
            _driver_buffer_audio_in[1] = ptr;

            ptr += _buffer_size_in_samples + DEVICE_CTRL_PKT_SIZE_WORDS;
            _tx_pkt[0] = reinterpret_cast<audio_ctrl::AudioCtrlPkt*>(ptr);

            ptr += AUDIO_CTRL_PKT_SIZE_WORDS;
            _driver_buffer_audio_out[0] = ptr;

            ptr += _buffer_size_in_samples + DEVICE_CTRL_PKT_SIZE_WORDS;
            _tx_pkt[1] = reinterpret_cast<audio_ctrl::AudioCtrlPkt*>(ptr);

            ptr += AUDIO_CTRL_PKT_SIZE_WORDS;
            _driver_buffer_audio_out[1] = ptr;
        }
        else
        {
            _driver_buffer_audio_in[0] = _driver_buffer;
            _driver_buffer_audio_in[1] = _driver_buffer +
                                         _buffer_size_in_samples;

            _driver_buffer_audio_out[0] = _driver_buffer_audio_in[1] +
                                          _buffer_size_in_samples;
            _driver_buffer_audio_out[1] = _driver_buffer_audio_out[0] +
                                          _buffer_size_in_samples;

            _driver_cv_out = reinterpret_cast<uint32_t*>(
                                _driver_buffer_audio_out[1] +
                                _buffer_size_in_samples);

            _driver_cv_in = _driver_cv_out + 1;
        }

        _clear_driver_buffers();
    }

    /**
     * @brief Create audio buffers for the user.
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _init_user_buffers()
    {
        _user_buffers_allocated = false;
        int res = posix_memalign(reinterpret_cast<void**>(&_user_audio_in),
                                 16,
                                 _buffer_size_in_samples * sizeof(float)) ||
                  posix_memalign(reinterpret_cast<void**>(&_user_audio_out),
                                 16,
                                 _buffer_size_in_samples * sizeof(float));

        std::fill_n(_user_audio_in, _buffer_size_in_samples, 0);
        std::fill_n(_user_audio_out, _buffer_size_in_samples, 0);

        if (res < 0)
        {
            _raspa_error_code.set_error_val(RASPA_EUSER_BUFFERS, res);
            return -RASPA_EUSER_BUFFERS;
        }

        _user_buffers_allocated = true;
        return RASPA_SUCCESS;
    }

    /**
     * @brief Free up the allocated user buffers.
     */
    void _free_user_buffers()
    {
        if (_user_buffers_allocated)
        {
            free(_user_audio_in);
            free(_user_audio_out);
            _user_buffers_allocated = false;
        }
    }

    /**
     * @brief Initialize the sample converter instance.
     */
    void _init_sample_converter()
    {
        _sample_converter = get_sample_converter(_codec_format,
                                                 _buffer_size_in_frames,
                                                 _num_codec_chans);
    }

    /**
     * @brief Initialize delay error filter object
     */
    void _init_delay_error_filter()
    {
        _delay_error_filter = std::make_unique<RaspaDelayErrorFilter>(
                            DELAY_FILTER_SETTLING_CONSTANT);
    }

    /**
     * @brief Init the gpio com object.
     *
     * @return int RASPA_SUCCESS upon success, different error code otherwise.
     */
    int _init_gpio_com()
    {
        _gpio_com = std::make_unique<RaspaGpioCom>(SENSEI_SOCKET,
                                                   &_raspa_error_code);
        return _gpio_com->init();
    }

    /**
     * @brief De init the sample converter instance.
     */
    void _deinit_sample_converter()
    {
        _sample_converter.reset();
    }

    /**
     * @brief Deinit delay error filter instance
     */
    void _deinit_delay_error_filter()
    {
        _delay_error_filter.reset();
    }

    /**
     * @brief Deinit the gpio com instance
     */
    void _deinit_gpio_com()
    {
        _gpio_com.reset();
    }

    /**
     * @brief Stops the real time task.
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _stop_rt_task()
    {
        if (_task_started)
        {
            auto res = pthread_cancel(_processing_task);

            res |= __RASPA(pthread_join(_processing_task, NULL));

            _task_started = false;
            if (res < 0)
            {
                _raspa_error_code.set_error_val(RASPA_ETASK_CANCEL, res);
                return -RASPA_ETASK_CANCEL;
            }
        }

        return RASPA_SUCCESS;
    }

    /**
     * @brief Free up memory, delete instances and stops the rt thread
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _cleanup()
    {
        // The order is very important. Its the reverse order of instantiation,
        auto res = _stop_rt_task();
        _free_user_buffers();
        res |= _release_driver_buffers();
        res |= _close_device();

        _deinit_sample_converter();

        if (_platform_type == driver_conf::PlatformType::SYNC)
        {
            _deinit_delay_error_filter();
        }
        if (_platform_type != driver_conf::PlatformType::NATIVE)
	{
            _deinit_gpio_com();
	}

        return res;
    }

    /**
     * @brief Clear the driver buffers
     */
    void _clear_driver_buffers()
    {
        if (!_mmap_initialized)
        {
            return;
        }

        if (_platform_type != driver_conf::PlatformType::NATIVE)
        {
            audio_ctrl::clear_audio_ctrl_pkt(_rx_pkt[0]);
            audio_ctrl::clear_audio_ctrl_pkt(_rx_pkt[1]);
            audio_ctrl::clear_audio_ctrl_pkt(_tx_pkt[0]);
            audio_ctrl::clear_audio_ctrl_pkt(_tx_pkt[1]);
        }

        for (int i = 0; i < _buffer_size_in_samples; i++)
        {
            _driver_buffer_audio_out[0][i] = 0;
            _driver_buffer_audio_out[1][i] = 0;
            _driver_buffer_audio_in[0][i] = 0;
            _driver_buffer_audio_in[1][i] = 0;
        }
    }

    /**
     * @brief This helper function called in real time context is used for
     *        PlatformType::SYNC where raspa uses the delay error filter
     *        to synchronize with the external micro-controller. The filter is
     *        processed every call but the output is downsampled by
     *        DELAY_FILTER_DOWNSAMPLE_RATE. The output of the function is the
     *        amount of correction needed for the sleep period of the rt task.
     * @param timing_error_ns the timing error in ns
     * @return if called for DELAY_FILTER_DOWNSAMPLE_RATE times it returns the
     *         correction in ns for the sleep period, else it returns 0
     */
    int32_t _process_timing_error_with_downsampling(int32_t timing_error_ns)
    {
        auto correction_in_ns = _delay_error_filter->delay_error_filter_tick(
                            timing_error_ns);

        // downsampling logic
        _error_filter_process_count++;
        if (_error_filter_process_count < DELAY_FILTER_DOWNSAMPLE_RATE)
        {
            return 0;
        }

        _error_filter_process_count = 0;
        return correction_in_ns;
    }

    /**
     * @brief Helper function to perform user callback.
     *
     * @param input_samples The buffer containing input samples from the codec
     * @param output_samples The buffer containing samples to be sent to the
     * codec
     */
    void _perform_user_callback(int32_t* input_samples, int32_t* output_samples)
    {
        _sample_converter->codec_format_to_float32n(_user_audio_in,
                                                    input_samples);
        _user_callback(_user_audio_in, _user_audio_out, _user_data);
        _sample_converter->float32n_to_codec_format(output_samples,
                                                    _user_audio_out);
    }

    /**
     * @brief Prepares current tx audio packet with GPIO data as payload.
     *        It fetches GPIO data from the gpio com task and inserts it
     *        into the payload
     *
     * @param pkt The packet which is meant to contain the gpio command and data
     */
    void _prepare_gpio_cmd_pkt(audio_ctrl::AudioCtrlPkt* const pkt)
    {
        int num_blobs = 0;
        audio_ctrl::GpioDataBlob* data = pkt->payload.gpio_data_blob;

        // clear packet first
        audio_ctrl::create_default_audio_ctrl_pkt(pkt);

        // retreive packets from com task and insert into audio packet payload
        while (num_blobs < AUDIO_CTRL_PKT_MAX_NUM_GPIO_DATA_BLOBS &&
               _gpio_com->get_gpio_data_from_nrt(data[num_blobs]))
        {
            num_blobs++;
        }

        audio_ctrl::prepare_gpio_cmd_pkt(pkt, num_blobs);
    }

    /**
     * @brief Parse an rx packet and perform the necessary operations
     *
     * @param pkt The rx pkt to be parsed.
     */
    void _parse_rx_pkt(const audio_ctrl::AudioCtrlPkt* const pkt)
    {
        if (audio_ctrl::check_audio_pkt_for_magic_words(pkt) == 0)
        {
            return;
        }

        // Check if packet contains gpio packets. If so, send it to gpio com
        auto num_blobs = audio_ctrl::check_for_gpio_data(pkt);
        if (num_blobs > 0)
        {
            for (int i = 0; i < num_blobs; i++)
            {
                const audio_ctrl::GpioDataBlob& data =
                                    pkt->payload.gpio_data_blob[i];
                _gpio_com->send_gpio_data_to_nrt(data);
            }

            return;
        }

        auto num_midi_bytes = check_for_midi_data(pkt);
        if (num_midi_bytes > 0)
        {
            // TODO : process midi data
        }
    }

    /**
     * @brief Generates the next tx pkt. This decides what the next packet
     *        should be and the data it should contain.
     *
     * @param pkt The packet where new tx packet info and data will be
     *            inserted
     */
    void _get_next_tx_pkt_data(audio_ctrl::AudioCtrlPkt* pkt)
    {
        if (_stop_request_flag)
        {
            audio_ctrl::prepare_audio_cease_pkt(pkt, _audio_packet_seq_num);
            return;
        }

        // if gpio packets need to be sent, then pack payload with them
        if (_gpio_com->rx_gpio_data_available())
        {
            _prepare_gpio_cmd_pkt(pkt);
            return;
        }

        // Create default packet if nothing is there to be sent.
        audio_ctrl::create_default_audio_ctrl_pkt(pkt);

        // TODO : round robin between gpio and midi data
    }

    /**
     * @brief Main real time loop when platform type is native
     */
    void _rt_loop_native()
    {
        while (true)
        {
            auto res = __RASPA_IOCTL_RT(ioctl(_device_handle,
                                RASPA_IRQ_WAIT, &_buf_idx));
            if (res)
            {
                break;
            }

            if (_break_on_mode_sw && _interrupts_counter > 1)
            {
#ifdef RASPA_WITH_EVL
                // todo use evl_set_thread_mode()
#else
                pthread_setmode_np(0, PTHREAD_WARNSW, NULL);
                _break_on_mode_sw = 0;
#endif
            }

            // clear driver buffers if stop is requested
            if (_stop_request_flag)
            {
                _clear_driver_buffers();
            }
            else
            {
                _user_gate_in = *_driver_cv_in;
                _perform_user_callback(_driver_buffer_audio_in[_buf_idx],
                                       _driver_buffer_audio_out[_buf_idx]);
                *_driver_cv_out = _user_gate_out;
            }

            __RASPA_IOCTL_RT(ioctl(_device_handle, RASPA_USERPROC_FINISHED,
                                    NULL));
            _interrupts_counter++;
        }
    }

    /**
     * @brief main rt loop when platform type is asynchronous
     */
    void _rt_loop_async()
    {
        while (true)
        {
            auto res = __RASPA_IOCTL_RT(ioctl(_device_handle,
                                RASPA_IRQ_WAIT, &_buf_idx));
            if (res)
            {
                break;
            }

            if (_break_on_mode_sw && _interrupts_counter > 1)
            {
#ifdef RASPA_WITH_EVL
                // todo use evl_set_thread_mode()
#else
                pthread_setmode_np(0, PTHREAD_WARNSW, NULL);
                _break_on_mode_sw = 0;
#endif
            }

            // Store CV gate in
            _user_gate_in = audio_ctrl::get_gate_in_val(_rx_pkt[_buf_idx]);

            _parse_rx_pkt(_rx_pkt[_buf_idx]);
            _perform_user_callback(_driver_buffer_audio_in[_buf_idx],
                                   _driver_buffer_audio_out[_buf_idx]);
            _get_next_tx_pkt_data(_tx_pkt[_buf_idx]);

            // Set gate out info in tx packet
            audio_ctrl::set_gate_out_val(_tx_pkt[_buf_idx], _user_gate_out);

            __RASPA_IOCTL_RT(ioctl(_device_handle,
                           RASPA_USERPROC_FINISHED,
                           NULL));
            _interrupts_counter++;
        }
    }

    /**
     * @brief Main rt loop when platform type is synchronous
     */
    void _rt_loop_sync()
    {
        // do not perform userspace callback before delay filter is settled
        while (_interrupts_counter < DELAY_FILTER_SETTLING_CONSTANT)
        {
            auto res = __RASPA_IOCTL_RT(ioctl(_device_handle,
                                RASPA_IRQ_WAIT, &_buf_idx));
            if (res)
            {
                break;
            }

            // Timing error
            auto timing_error_ns =
                                audio_ctrl::get_timing_error(_rx_pkt[_buf_idx]);
            auto correction_ns = _process_timing_error_with_downsampling(
                                timing_error_ns);

            _parse_rx_pkt(_rx_pkt[_buf_idx]);
            _get_next_tx_pkt_data(_tx_pkt[_buf_idx]);

            res = __RASPA_IOCTL_RT(ioctl(_device_handle,
                           RASPA_USERPROC_FINISHED,
                           &correction_ns));
            if (res)
            {
                break;
            }
            _interrupts_counter++;
        }

        // main run time loop
        while (true)
        {
            auto res = __RASPA_IOCTL_RT(ioctl(_device_handle,
                                RASPA_IRQ_WAIT, &_buf_idx));
            if (res)
            {
                break;
            }

            // Timing error
            auto timing_error_ns =
                                audio_ctrl::get_timing_error(_rx_pkt[_buf_idx]);
            auto correction_ns = _process_timing_error_with_downsampling(
                                timing_error_ns);

            // Store CV gate in
            _user_gate_in = audio_ctrl::get_gate_in_val(_rx_pkt[_buf_idx]);

            _parse_rx_pkt(_rx_pkt[_buf_idx]);
            _perform_user_callback(_driver_buffer_audio_in[_buf_idx],
                                   _driver_buffer_audio_out[_buf_idx]);
            _get_next_tx_pkt_data(_tx_pkt[_buf_idx]);

            // Set gate out info in tx packet
            audio_ctrl::set_gate_out_val(_tx_pkt[_buf_idx], _user_gate_out);

            res = __RASPA_IOCTL_RT(ioctl(_device_handle,
                           RASPA_USERPROC_FINISHED,
                           &correction_ns));
            if (res)
            {
                break;
            }
            _interrupts_counter++;
        }
    }

    // Pointers for driver data
    int32_t* _driver_buffer;
    int32_t* _driver_buffer_audio_in[NUM_BUFFERS];
    int32_t* _driver_buffer_audio_out[NUM_BUFFERS];
    uint32_t* _driver_cv_in;
    uint32_t* _driver_cv_out;
    audio_ctrl::AudioCtrlPkt* _tx_pkt[NUM_BUFFERS];
    audio_ctrl::AudioCtrlPkt* _rx_pkt[NUM_BUFFERS];
    size_t _kernel_buffer_mem_size;

    // User buffers for audio
    float* _user_audio_in;
    float* _user_audio_out;
    uint32_t _user_gate_in;
    uint32_t _user_gate_out;
    int _buf_idx;

    // device handle identifier
    int _device_handle;

    // counter to count the number of interrupts
    int _interrupts_counter;

    // flag to denote that a stop has been requested
    bool _stop_request_flag;

    // flag to break on mode switch occurrence
    bool _break_on_mode_sw;

    // audio buffer parameters
    float _sample_rate;
    int _num_codec_chans;
    int _num_input_chans;
    int _num_output_chans;
    int _buffer_size_in_frames;
    int _buffer_size_in_samples;
    driver_conf::CodecFormat _codec_format;
    std::unique_ptr<BaseSampleConverter> _sample_converter;

    // initialization phases
    bool _device_opened;
    bool _user_buffers_allocated;
    bool _mmap_initialized;
    bool _task_started;

    // rt task data
    void* _user_data;
    RaspaProcessCallback _user_callback;
    pthread_t _processing_task;

    // Error code helper class
    RaspaErrorCode _raspa_error_code;

    // Raspa platform type
    driver_conf::PlatformType _platform_type;

    // Delay filter
    std::unique_ptr<RaspaDelayErrorFilter> _delay_error_filter;
    int _error_filter_process_count;

    // Gpio Comm
    std::unique_ptr<RaspaGpioCom> _gpio_com;

    // seq number for audio control packets
    uint32_t _audio_packet_seq_num;
};

static void* raspa_pimpl_task_entry(void* data)
{
    auto pimpl = static_cast<RaspaPimpl*>(data);
    int efd = evl_attach_self("/raspa_pimpl_task:%d", getpid());
    if (efd < 0)
    {
        error(1, -efd, "evl_attach_self() failed");
    }

    pimpl->rt_loop();

    // To suppress warnings
    return nullptr;
}

}  // namespace raspa

#endif  // RASPA_RASPA_PIMPL_H
