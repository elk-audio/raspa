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
 * @brief Contains macros, enums and functions that help in interfacing with the
 *        audio driver and its parameters
 * @copyright 2017-2021 Modern Ancient Instruments Networked AB, dba Elk,
 * Stockholm
 */

#ifndef RASPA_DRIVER_CONFIG_H
#define RASPA_DRIVER_CONFIG_H

#include <fcntl.h>

#include <cerrno>
#include <string>
#include <utility>

#define RASPA_IOC_MAGIC 'r'

#define RASPA_IRQ_WAIT          _IO(RASPA_IOC_MAGIC, 1)
#define RASPA_PROC_START        _IO(RASPA_IOC_MAGIC, 3)
#define RASPA_USERPROC_FINISHED _IOW(RASPA_IOC_MAGIC, 4, int)
#define RASPA_PROC_STOP         _IO(RASPA_IOC_MAGIC, 5)

#define RASPA_PROCESSING_TASK_PRIO 90

namespace driver_conf {

/**
 * required driver versions
 */
constexpr int REQUIRED_MAJ_VER = 0;
constexpr int REQUIRED_MIN_VER = 3;

/**
 * device paths
 */
constexpr char DEVICE_NAME[] = "/dev/rtdm/audio_rtdm";

// Driver parameter definitions
constexpr char PARAM_ROOT_PATH[] = "/sys/class/audio_rtdm/";
constexpr size_t PARAM_VAL_STR_LEN = 25;

constexpr char SAMPLE_RATE_PARAM[] = "audio_sampling_rate";
constexpr char NUM_INPUT_CHANS_PARAM[] = "audio_input_channels";
constexpr char NUM_OUTPUT_CHANS_PARAM[] = "audio_output_channels";
constexpr char CODEC_FORMAT_PARAM[] = "audio_format";
constexpr char BUFFER_SIZE_PARAM[] = "audio_buffer_size";
constexpr char PLATFORM_TYPE_PARAM[] = "platform_type";
constexpr char MAJ_VER_PARAM[] = "audio_rtdm_ver_maj";
constexpr char MIN_VER_PARAM[] = "audio_rtdm_ver_min";

/**
 * @brief Enumeration to denote various codec sample formats
 */
enum class CodecFormat : int
{
    INT24_LJ = 1,  // 24 bit samples left justified. Format : 0xXXXXXX00
    INT24_I2S,     // 24 bit samples I2S format (first bit is 0). Format:
                   // 0xXXXXXX00
    INT24_RJ,      // 24 bit samples right justified. Format : 0x00XXXXXX
    INT24_32RJ,    // 24 bit samples converted into 32 bit samples
    INT32,         // 32 bit samples
    NUM_CODEC_FORMATS
};

/**
 * @brief Enumeration to denote the platform type.
 * NATIVE : Indicates that the host machine directly interfaces with the codec
 *          and GPIO system.
 * ASYNC  : Indicates that a secondary controller interfaces with the codec and
 *          GPIO system and the host machine interacts with it asynchronously.
 * SYNC   : Indicates that a secondary controller interfaces with the codec and
 *          GPIO system and the host machine interacts with it synchronously.
 */
enum class PlatformType : int
{
    NATIVE = 1,
    SYNC,
    ASYNC
};

/**
 * @brief Enumeration of custom error codes that the driver can return.
 *        DEVICE_INACTIVE: denotes inactivity of microcontroller in SYNC and
 *                         ASYNC platforms.
 *        INVALID_FIRMWARE_VER: denotes that the microcontroller in SYNC and
 *                              ASYNC platforms have invalid firmware version.
 *        INVALID_BUFFER_SIZE: denotes that the driver does not support the
 *                             configured buffer size
 */
enum class ErrorCode : int
{
    DEVICE_INACTIVE = 140,
    INVALID_FIRMWARE_VER,
    INVALID_BUFFER_SIZE
};

/**
 * @brief Read driver params as int value
 *
 * @param param_name The param name
 * @return int negative error code upon failure, >=0 upon success
 */
int read_driver_param(const char* param_name)
{
    auto param_path = std::string(PARAM_ROOT_PATH) + param_name;
    std::string param_str(PARAM_VAL_STR_LEN, '\0');

    auto fd = open(param_path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        // failed to open
        return fd;
    }

    auto res = read(fd, param_str.data(), PARAM_VAL_STR_LEN);
    close(fd);

    if (res < 0)
    {
        // read operation failed
        return errno;
    }

    // Using atoi for no exception guarantee
    return std::atoi(param_str.c_str());
}

/**
 * Helper functions to set and get various driver params
 */
int get_sample_rate()
{
    return read_driver_param(SAMPLE_RATE_PARAM);
}

int get_num_input_chan()
{
    return read_driver_param(NUM_INPUT_CHANS_PARAM);
}

int get_num_output_chan()
{
    return read_driver_param(NUM_OUTPUT_CHANS_PARAM);
}

int get_codec_format()
{
    return read_driver_param(CODEC_FORMAT_PARAM);
}

int get_platform_type()
{
    return read_driver_param(PLATFORM_TYPE_PARAM);
}

int get_buffer_size()
{
    return read_driver_param(BUFFER_SIZE_PARAM);
}

std::pair<bool, int> check_driver_version()
{
    auto major_ver = read_driver_param(MAJ_VER_PARAM);
    auto minor_ver = read_driver_param(MIN_VER_PARAM);

    if (major_ver < 0)
    {
        return {false, major_ver};
    }

    if (minor_ver < 0)
    {
        return {false, minor_ver};
    }

    if (major_ver != REQUIRED_MAJ_VER || minor_ver < REQUIRED_MIN_VER)
    {
        return {false, 0};
    }

    return {true, 0};
}

}  // namespace driver_conf

#endif  // RASPA_DRIVER_CONFIG_H
