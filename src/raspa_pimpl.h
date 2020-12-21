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
 * @brief Definition of class RaspaPimpl, which abstracts low-level access
 *        to a RTDM Audio device from userspace in scenarios where the driver
 *        directly interfaces with the CODECS. Provides access to RT driver
 *        through a typical callback registration service. This class provides a
 *        private implementation of the api found in raspa.h
 * @copyright 2017-2020 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */
#ifndef RASPA_PIMPL_H_
#define RASPA_PIMPL_H_

#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include <sched.h>
#include <vector>
#include <fstream>

#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <rtdm/rtdm.h>
#include <xenomai/init.h>
#include <cobalt/pthread.h>
#include <cobalt/time.h>
#include <cobalt/sys/ioctl.h>

#pragma GCC diagnostic pop

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <string>

#include "raspa/raspa.h"
#include "driver_config.h"
#include "raspa_error_codes.h"
#include "sample_conversion.h"
#include "raspa_delay_error_filter.h"
#include "raspa_gpio_com.h"
#include "audio_control_protocol/audio_control_protocol.h"
#include "audio_control_protocol/audio_packet_helper.h"

#ifdef RASPA_DEBUG_PRINT
#include <stdio.h>
#endif

/*
 * This variable is defined by xenomai init. It is used to index the number
 * of command line arguments passed to xenomai. Since these arguments are passed
 * manually, this variable is incremented in the init() function.
 */
extern int optind;

namespace {

// Delay in microseconds to wait for audio driver to close and stop its thread.
constexpr int CLOSE_DELAY_US = 500000;

// Delay in microseconds for a stop request to be processed
constexpr int STOP_REQUEST_DELAY_US = 10000;

constexpr int THREAD_CREATE_DELAY_US = 10000;

// Number of kernel memory pages raspa allocates
constexpr int NUM_PAGES_KERNEL_MEM = 20;

//Num of audio buffers.
constexpr int NUM_BUFFERS = 2;

// Driver parameter definitions
constexpr int DRIVER_PARAM_PATH_LEN = 100;
constexpr int DRIVER_PARAM_VAL_STR_LEN = 25;

/**
 * driver versions
 */
constexpr int REQUIRED_DRIVER_VERSION_MAJ = 0;
constexpr int REQUIRED_DRIVER_VERSION_MIN = 2;

// settling constant for the delay filter
constexpr int DELAY_FILTER_SETTLING_CONSTANT = 100;

// Down sampling rate for the delay filter
constexpr int DELAY_FILTER_DOWNSAMPLE_RATE = 16;

// Sensei socket address
constexpr char SENSEI_SOCKET[] = "/tmp/sensei";

constexpr int NUM_DATA_POINTS = 1000;
}

namespace raspa {

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
    RaspaPimpl() : _driver_buffer(nullptr),
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
                   _sample_rate(0),
                   _num_codec_chans(0),
                   _num_input_chans(0),
                   _num_output_chans(0),
                   _buffer_size_in_frames(0),
                   _buffer_size_in_samples(0),
                   _codec_format(RaspaCodecFormat::INT24_LJ),
                   _device_opened(false),
                   _user_buffers_allocated(false),
                   _mmap_initialized(false),
                   _task_started(false),
                   _user_data(nullptr),
                   _user_callback(nullptr),
                   _platform_type(RaspaPlatformType::NATIVE),
                   _error_filter_process_count(0),
                   _audio_packet_seq_num(0)
    {}

    ~RaspaPimpl()
    {
        _cleanup();
    }

    int init()
    {
        /*
         * Fake command line arguments to pass to xenomai_init(). For some
         * obscure reasons, xenomai_init() crashes if argv is allocated here on
         * the stack, so we alloc it beforehand.
         */
        int argc = 2;
        auto argv = new char* [argc + 1];
        for (int i = 0; i < argc; i++)
        {
            argv[i] = new char[32];
        }
        argv[argc] = nullptr;

        std::strcpy(argv[0], "raspa");

        // add cpu affinity argument to xenomai init setting it to all cores.
        std::strcpy(argv[1], "--cpu-affinity=");
        for (int i = 0; i < get_nprocs(); i++)
        {
            char arg_cpu_num[2];
            std::sprintf(arg_cpu_num, "%d", i);

            // add cpu number to the list
            std::strncat(argv[1], arg_cpu_num, 2);

            // add comma except for last cpu number
            if (i != get_nprocs() - 1)
            {
                std::strncat(argv[1], ",", 2);
            }
        }

        optind = 1;

        xenomai_init(&argc, (char* const**) &argv);
        _kernel_buffer_mem_size = NUM_PAGES_KERNEL_MEM * getpagesize();

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

        return RASPA_SUCCESS;
    }

    int open(int buffer_size,
             RaspaProcessCallback process_callback,
             void* user_data, unsigned int debug_flags)
    {
        auto res = _get_module_param_path();
        if (res != RASPA_SUCCESS)
        {
            return res;
        }

        res = _get_audio_info_from_driver();
        if (res != RASPA_SUCCESS)
        {
            return res;
        }

        _buffer_size_in_frames = buffer_size;
        res = _check_driver_compatibility();
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
            return -RASPA_EINVALID_BUFFSIZE;
        }

        // Delay filter is needed for synchronization
        if (_platform_type == RaspaPlatformType::SYNC)
        {
            _init_delay_error_filter();
        }

        if (_platform_type != RaspaPlatformType::NATIVE)
        {
            auto res = _init_gpio_com();
            if (res != RASPA_SUCCESS)
            {
                return res;
            }
        }

        _user_data = user_data;
        _interrupts_counter = 0;
        _user_callback = process_callback;


        wakeup_ts.resize(NUM_DATA_POINTS, 0);
        sleep_period.resize(NUM_DATA_POINTS, 0);
        timing_error.resize(NUM_DATA_POINTS, 0);
        timing_correction.resize(NUM_DATA_POINTS, 0);

        data_point = 0;

        return RASPA_SUCCESS;
    }

    int start_realtime()
    {
        // Initialize RT task
        _task_started = false;
        struct sched_param rt_params = {.sched_priority = RASPA_PROCESSING_TASK_PRIO};
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
        auto res = pthread_attr_setaffinity_np(&task_attributes,
                                               sizeof(cpu_set_t), &cpuset);
        if (res < 0)
        {
            _cleanup();
            _raspa_error_code.set_error_val(RASPA_ETASK_AFFINITY, res);
            return -RASPA_ETASK_AFFINITY;
        }

        // Create rt thread
        res = __cobalt_pthread_create(&_processing_task, &task_attributes,
                                      &raspa_pimpl_task_entry, this);
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

        res = __cobalt_ioctl(_device_handle, RASPA_PROC_START);
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
        switch(_platform_type)
        {
        case RaspaPlatformType::NATIVE:
            _rt_loop_native();
            break;

        case RaspaPlatformType::SYNC:
            _rt_loop_sync();
            break;

        case RaspaPlatformType::ASYNC:
            _rt_loop_async();
            break;
        }

        pthread_exit(nullptr);
    }

    float get_sampling_rate()
    {
        return (float) _sample_rate;
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
        auto res = __cobalt_clock_gettime(CLOCK_MONOTONIC, &tp);
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

    int close()
    {
        _stop_request_flag = true;

        // Wait sometime for periodic task to send mute command to device
        usleep(STOP_REQUEST_DELAY_US);

        auto res = __cobalt_ioctl(_device_handle, RASPA_PROC_STOP);

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


protected:
    /**
     * @brief Read driver parameter
     * @param param_name The parameter to read
     * @return integer value of the parameter upon success, linux error code
     *         otherwise.
     */
    int _read_driver_param(const char* param_name)
    {
        int rtdm_file;
        std::string full_param_path = _module_param_path + "/" + param_name;
        char value[DRIVER_PARAM_VAL_STR_LEN];

        rtdm_file = __cobalt_open(full_param_path.c_str(), O_RDONLY);

        if (rtdm_file < 0)
        {
            return rtdm_file;
        }

        if (read(rtdm_file, value, DRIVER_PARAM_VAL_STR_LEN) == -1)
        {
            return rtdm_file;
        }

        __cobalt_close(rtdm_file);

        return atoi(value);
    }

    /**
     * @brief Checks compatibility of raspa configuration with that of the
     *        driver.
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _check_driver_compatibility()
    {
        auto buffer_size = _read_driver_param("audio_buffer_size");
        auto major_version = _read_driver_param("audio_ver_maj");
        auto minor_version = _read_driver_param("audio_ver_min");

        if (buffer_size < 0 || major_version < 0 || minor_version < 0)
        {
            return -RASPA_EPARAM;
        }

        if (_buffer_size_in_frames != buffer_size)
        {
            return -RASPA_EBUFFSIZE;
        }

        if (REQUIRED_DRIVER_VERSION_MAJ != major_version)
        {
            return -RASPA_EVERSION;
        }


        if (REQUIRED_DRIVER_VERSION_MIN != minor_version)
        {
            return -RASPA_EVERSION;
        }

        return RASPA_SUCCESS;
    }

    /**
    * @brief Find the module dir in RASPA_MODULE_PARAM_ROOT_PATH. The module dir
    *        name will always have the suffix RASPA_MODULE_NAME_SUFFIX.
    * @return RASPA_SUCCES upon success, -RASPA_EPARAM if module is not found
    */
    int _get_module_param_path()
    {
        std::string module_root_path(RASPA_MODULE_PARAM_ROOT_PATH);
        std::string module_name_suffix(RASPA_MODULE_NAME_SUFFIX);

        for (auto &p : std::filesystem::directory_iterator(module_root_path))
        {
            const auto& dir_name = p.path().stem().string();

            /* search for suffix RASPA_MODULE_NAME_SUFFIX in dir name. If found
               then this is the module directory name */
            auto found = dir_name.find(module_name_suffix);
            if (found != std::string::npos)
            {
                _module_param_path = module_root_path + dir_name + "/parameters";
                return RASPA_SUCCESS;
            }
        }

        return -RASPA_EPARAM;
    }

    /**
     * @brief Get the various info from the drivers parameter
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _get_audio_info_from_driver()
    {
        _sample_rate = _read_driver_param("audio_sampling_rate");
        _num_input_chans = _read_driver_param("audio_input_channels");
        _num_output_chans = _read_driver_param("audio_output_channels");
        auto codec_format = _read_driver_param("audio_format");
        auto platform_type = _read_driver_param("platform_type");

        if (_sample_rate < 0 || _num_output_chans < 0
            || _num_output_chans < 0 || codec_format < 0 || platform_type < 0)
        {
            return -RASPA_EPARAM;
        }

        if (codec_format < static_cast<int>(RaspaCodecFormat::INT24_LJ)
        || codec_format >= static_cast<int>(RaspaCodecFormat::NUM_CODEC_FORMATS))
        {
            _raspa_error_code.set_error_val(RASPA_ECODEC_FORMAT, codec_format);
            return -RASPA_ECODEC_FORMAT;
        }
        _codec_format = static_cast<RaspaCodecFormat>(codec_format);

        if (platform_type < static_cast<int>(RaspaPlatformType::NATIVE)
        || platform_type > static_cast<int>(RaspaPlatformType::ASYNC))
        {
            _raspa_error_code.set_error_val(RASPA_EPLATFORM_TYPE, platform_type);
        }
        _platform_type = static_cast<RaspaPlatformType>(platform_type);

        _num_codec_chans = (_num_input_chans > _num_output_chans)
                           ? _num_input_chans : _num_output_chans;

        return RASPA_SUCCESS;
    }

    /**
     * @brief Open the rtdm device.
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise
     */
    int _open_device()
    {
        _device_opened = false;
        _device_handle = __cobalt_open(RASPA_DEVICE_NAME, O_RDWR);

        if (_device_handle < 0)
        {
            // check if it is external micro-controller related issues
            switch (_device_handle)
            {
            case RASPA_ERROR_CODE_DEVICE_INACTIVE:
                return -RASPA_EDEVICE_INACTIVE;
                break;

            case RASPA_ERROR_CODE_FIRMARE_CHECK:
                return -RASPA_EDEVICE_FIRMWARE;
                break;

            default:
                _raspa_error_code.set_error_val(RASPA_EDEVICE_OPEN, _device_handle);
                return -RASPA_EDEVICE_OPEN;
            }
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
            auto res = __cobalt_close(_device_handle);
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
        _driver_buffer = (int32_t*) __cobalt_mmap(NULL, _kernel_buffer_mem_size,
                                                  PROT_READ | PROT_WRITE,
                                                  MAP_SHARED, _device_handle,
                                                  0);
        if (_driver_buffer == MAP_FAILED)
        {
            _raspa_error_code.set_error_val(RASPA_ENOMEM, errno);
            return -RASPA_ENOMEM;
        }

        _mmap_initialized = true;
        return RASPA_SUCCESS;
    }

    /**
     * @brief Unmaps the acquired audio buffers from the driver.
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
     *        For RaspaPlatformType::NATIVE:
     *        1. audio buffer in number 0
     *        2. audio buffer in number 1
     *        3. audio buffer out number 0
     *        4. audio buffer out number 1
     *
     *        For other platform types
     *        1. rx audio control packet number 0
     *        2. audio buffer in number 0
     *        3. rx audio control packet number 1
     *        4. audio buffer in number 1
     *        5. tx audio control packet number 0
     *        6. audio buffer out number 0
     *        7. tx audio control packet number 1
     *        8. audio buffer out number 1
     */
    void _init_driver_buffers()
    {
        _buffer_size_in_samples = _buffer_size_in_frames * _num_codec_chans;

        /* If raspa platform type is not native, then the driver buffers
         * also include space for audio control packet.
         */
        if (_platform_type != RaspaPlatformType::NATIVE)
        {
            _rx_pkt[0] = (audio_ctrl::AudioCtrlPkt*) _driver_buffer;

            _driver_buffer_audio_in[0] = _driver_buffer +
                                         AUDIO_CTRL_PKT_SIZE_WORDS;

            _rx_pkt[1] = (audio_ctrl::AudioCtrlPkt*) (_driver_buffer_audio_in[0]
                                                     + _buffer_size_in_samples);

            _driver_buffer_audio_in[1] = ((int32_t*) _rx_pkt[1]) +
                                         AUDIO_CTRL_PKT_SIZE_WORDS;

            _tx_pkt[0] = (audio_ctrl::AudioCtrlPkt*) (_driver_buffer_audio_in[1]
                                                     + _buffer_size_in_samples);

            _driver_buffer_audio_out[0] = ((int32_t *) _tx_pkt[0])
                                          + AUDIO_CTRL_PKT_SIZE_WORDS;

            _tx_pkt[1] = (audio_ctrl::AudioCtrlPkt*) (_driver_buffer_audio_out[0]
                                                     + _buffer_size_in_samples);

            _driver_buffer_audio_out[1] = ((int32_t *) _tx_pkt[1]) +
                                          AUDIO_CTRL_PKT_SIZE_WORDS;
        }
        else
        {
            _driver_buffer_audio_in[0] = _driver_buffer;
            _driver_buffer_audio_in[1] = _driver_buffer + _buffer_size_in_samples;

            _driver_buffer_audio_out[0] =
                    _driver_buffer_audio_in[1] + _buffer_size_in_samples;
            _driver_buffer_audio_out[1] =
                    _driver_buffer_audio_out[0] + _buffer_size_in_samples;

            _driver_cv_out = (uint32_t*) _driver_buffer_audio_out[1] +
                             _buffer_size_in_samples;
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
        int res = posix_memalign((void**) &_user_audio_in, 16,
                                 _buffer_size_in_samples * sizeof(float))
                  || posix_memalign((void**) &_user_audio_out, 16,
                                    _buffer_size_in_samples * sizeof(float));

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
        _delay_error_filter = std::make_unique<RaspaDelayErrorFilter>
                                               (DELAY_FILTER_SETTLING_CONSTANT);
    }

    /**
     * @brief Init the gpio com object.
     *
     * @return int RASPA_SUCCESS upon success, different error code otherwise.
     */
    int _init_gpio_com()
    {
        _gpio_com = std::make_unique<RaspaGpioCom>(SENSEI_SOCKET, &_raspa_error_code);
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
            res |= __cobalt_pthread_join(_processing_task, NULL);
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

        if (_platform_type == RaspaPlatformType::SYNC)
        {
            _deinit_delay_error_filter();
            _deinit_gpio_com();
        }

        std::ofstream outfile;
        outfile.open("timing_error.txt");
        for (int i = 0; i < data_point; i++)
        {
            outfile << wakeup_ts[i] << " " << timing_error[i] << " " << timing_correction[i] << std::endl;
        }
        printf("finished writing \n");
        outfile.close();

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

        if (_platform_type != RaspaPlatformType::NATIVE)
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
     *        RaspaPlatformType::SYNC where raspa uses the delay error filter
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
        auto correction_in_ns =
           _delay_error_filter->delay_error_filter_tick(timing_error_ns);

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
     * @param output_samples The buffer containing samples to be sent to the codec
     */
    void _perform_user_callback(int32_t* input_samples, int32_t* output_samples)
    {
        _sample_converter->codec_format_to_float32n
                           (_user_audio_in, input_samples);
        _user_callback(_user_audio_in, _user_audio_out, _user_data);
        _sample_converter->float32n_to_codec_format
                          (output_samples, _user_audio_out);
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
            auto buf_idx = __cobalt_ioctl(_device_handle, RASPA_IRQ_WAIT);
            if (buf_idx < 0)
            {
                // TODO: think how to handle this. Error here means something *bad*,
                // so we might want to de-register the driver, cleanup and signal
                // user someway.
                break;
            }

            if (_break_on_mode_sw && _interrupts_counter > 1)
            {
                pthread_setmode_np(0, PTHREAD_WARNSW, NULL);
                _break_on_mode_sw = 0;
            }

            // clear driver buffers if stop is requested
            if (_stop_request_flag)
            {
                _clear_driver_buffers();
            }
            else
            {
                _user_gate_in = *_driver_cv_in;
                _perform_user_callback(_driver_buffer_audio_in[buf_idx],
                                       _driver_buffer_audio_out[buf_idx]);
                *_driver_cv_out = _user_gate_out;
            }

            __cobalt_ioctl(_device_handle, RASPA_USERPROC_FINISHED, NULL);
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
            auto buf_idx = __cobalt_ioctl(_device_handle, RASPA_IRQ_WAIT);
            if (buf_idx < 0)
            {
                break;
            }

            if (_break_on_mode_sw && _interrupts_counter > 1)
            {
                pthread_setmode_np(0, PTHREAD_WARNSW, NULL);
                _break_on_mode_sw = 0;
            }

            // Store CV gate in
            _user_gate_in = audio_ctrl::get_gate_in_val(_rx_pkt[buf_idx]);

            _parse_rx_pkt(_rx_pkt[buf_idx]);
            _perform_user_callback(_driver_buffer_audio_in[buf_idx],
                                   _driver_buffer_audio_out[buf_idx]);
            _get_next_tx_pkt_data(_tx_pkt[buf_idx]);

            // Set gate out info in tx packet
            audio_ctrl::set_gate_out_val(_tx_pkt[buf_idx], _user_gate_out);

            __cobalt_ioctl(_device_handle, RASPA_USERPROC_FINISHED, NULL);
            _interrupts_counter++;
        };
    }

    /**
     * @brief Main rt loop when platform type is synchronous
     */
    void _rt_loop_sync()
    {
        struct timespec tp;

        // do not perform userspace callback before delay filter is settled
        while (_interrupts_counter < DELAY_FILTER_SETTLING_CONSTANT)
        {
            auto buf_idx = __cobalt_ioctl(_device_handle, RASPA_IRQ_WAIT);
            if (buf_idx < 0)
            {
                break;
            }

            // Timing error
            auto timing_error_ns =
                                audio_ctrl::get_timing_error(_rx_pkt[buf_idx]);
            auto correction_ns =
                _process_timing_error_with_downsampling(timing_error_ns);

            _parse_rx_pkt(_rx_pkt[buf_idx]);
            _get_next_tx_pkt_data(_tx_pkt[buf_idx]);

            __cobalt_ioctl(_device_handle, RASPA_USERPROC_FINISHED,
                           &correction_ns);
            _interrupts_counter++;
        }

        // main run time loop
        while (true)
        {
            auto buf_idx = __cobalt_ioctl(_device_handle, RASPA_IRQ_WAIT);
            if (buf_idx < 0)
            {
                break;
            }

            __cobalt_clock_gettime(CLOCK_MONOTONIC, &tp);

            // Timing error
            auto timing_error_ns =
                                audio_ctrl::get_timing_error(_rx_pkt[buf_idx]);
            auto correction_ns =
                _process_timing_error_with_downsampling(timing_error_ns);

            if (data_point < NUM_DATA_POINTS && _interrupts_counter > 5000)
            {
                timing_error[data_point] = timing_error_ns;
                timing_correction[data_point] = correction_ns;
                wakeup_ts[data_point] = (tp.tv_sec * 1000000000) + tp.tv_nsec;
                data_point++;
                if (data_point == NUM_DATA_POINTS)
                {
                    __cobalt_printf("Done collecting\n");
                }
            }

            // Store CV gate in
            _user_gate_in = audio_ctrl::get_gate_in_val(_rx_pkt[buf_idx]);

            _parse_rx_pkt(_rx_pkt[buf_idx]);
            _perform_user_callback(_driver_buffer_audio_in[buf_idx],
                                   _driver_buffer_audio_out[buf_idx]);
            _get_next_tx_pkt_data(_tx_pkt[buf_idx]);

            // Set gate out info in tx packet
            audio_ctrl::set_gate_out_val(_tx_pkt[buf_idx], _user_gate_out);

            __cobalt_ioctl(_device_handle, RASPA_USERPROC_FINISHED,
                           &correction_ns);
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

    // device handle identifier
    int _device_handle = -1;

    // To store the module paramenter path
    std::string _module_param_path;

    // counter to count the number of interrupts
    int _interrupts_counter;

    // flag to denote that a stop has been requested
    bool _stop_request_flag;

    // flag to break on mode switch occurence
    bool _break_on_mode_sw;

    // audio buffer parameters
    int _sample_rate;
    int _num_codec_chans;
    int _num_input_chans;
    int _num_output_chans;
    int _buffer_size_in_frames;
    int _buffer_size_in_samples;
    RaspaCodecFormat _codec_format;
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
    RaspaPlatformType _platform_type;

    // Delay filter
    std::unique_ptr<RaspaDelayErrorFilter> _delay_error_filter;
    int _error_filter_process_count;

    // Gpio Comm
    std::unique_ptr<RaspaGpioCom> _gpio_com;

    // seq number for audio control packets
    uint32_t _audio_packet_seq_num;

    std::vector<uint32_t> wakeup_ts;
    std::vector<uint32_t> sleep_period;
    std::vector<int32_t> timing_error;
    std::vector<int32_t> timing_correction;

    int data_point;
};

static void* raspa_pimpl_task_entry(void* data)
{
    auto raspa_pimpl = static_cast<RaspaPimpl*>(data);
    raspa_pimpl->rt_loop();

    // To suppress warnings
    return nullptr;
}

} // namespace raspa

#endif // RASPA_PIMPL_H
