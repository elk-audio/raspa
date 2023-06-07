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

#define RASPA_PROCESSING_TASK_PRIO 90

#define RASPA_IOC_MAGIC 'r'

#define RASPA_IRQ_WAIT              _IOR(RASPA_IOC_MAGIC, 1, int)
#define RASPA_PROC_START            _IO(RASPA_IOC_MAGIC, 3)
#define RASPA_USERPROC_FINISHED     _IOW(RASPA_IOC_MAGIC, 4, int)
#define RASPA_PROC_STOP             _IO(RASPA_IOC_MAGIC, 5)
#define RASPA_FW_TRANSFER           _IO(RASPA_IOC_MAGIC, 6)
#define RASPA_GPIO_GET_PIN          _IOW(RASPA_IOC_MAGIC, 7, RtGpio)
#define RASPA_GPIO_SET_DIR_OUT      _IOW(RASPA_IOC_MAGIC, 8, RtGpio)
#define RASPA_GPIO_SET_VAL          _IOW(RASPA_IOC_MAGIC, 9, RtGpio)
#define RASPA_GPIO_RELEASE          _IOW(RASPA_IOC_MAGIC, 10, RtGpio)
#define RASPA_GET_INPUT_CHAN_INFO   _IOWR(RASPA_IOC_MAGIC, 11, struct driver_conf::ChannelInfo)
#define RASPA_GET_OUTPUT_CHAN_INFO  _IOWR(RASPA_IOC_MAGIC, 12, struct driver_conf::ChannelInfo)


namespace driver_conf {

/**
 * required driver API versions. Rev version of driver is not relevant. Major
 * and minor versions dictate the support for the various IOCTLS and sysfs
 * params
 */
constexpr int REQUIRED_MAJ_VER = 1;
constexpr int REQUIRED_MIN_VER = 0;

/**
 * device paths
 */
#ifdef RASPA_WITH_EVL
constexpr char DEVICE_NAME[] = "/dev/audio_evl";
#else
constexpr char DEVICE_NAME[] = "/dev/rtdm/audio_rtdm";
#endif

// Driver parameter definitions
#ifdef RASPA_WITH_EVL
constexpr char PARAM_ROOT_PATH[] = "/sys/class/audio_evl/";
#else
constexpr char PARAM_ROOT_PATH[] = "/sys/class/audio_rtdm/";
#endif
constexpr size_t PARAM_VAL_STR_LEN = 25;

constexpr char SAMPLE_RATE_PARAM[] = "audio_sampling_rate";
constexpr char NUM_INPUT_CHANS_PARAM[] = "audio_input_channels";
constexpr char NUM_OUTPUT_CHANS_PARAM[] = "audio_output_channels";
constexpr char BUFFER_SIZE_PARAM[] = "audio_buffer_size";
constexpr char PLATFORM_TYPE_PARAM[] = "platform_type";
constexpr char MAJ_VER_PARAM[] = "audio_ver_maj";
constexpr char MIN_VER_PARAM[] = "audio_ver_min";
constexpr char USB_AUDIO_TYPE_PARAM[] = "usb_audio_type";
constexpr char IRQ_AFFINITY[] = "audio_irq_affinity";

/**
 * @brief Enumeration to denote various codec sample formats
 */
enum class CodecFormat : int
{
    NONE = 0,
    INT24_LJ = 1,  // 24 bit samples left justified. Format : 0xXXXXXX00
    INT24_I2S,     // 24 bit samples I2S format (first bit is 0). Format:
                   // 0xXXXXXX00
    INT24_RJ,      // 24 bit samples right justified. Format : 0x00XXXXXX
    INT24_32RJ,    // 24 bit samples converted into 32 bit samples
    INT32,         // 32 bit samples
    BINARY,         // No op to be done on samples
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
 *                             configured buffer size.
 *        INVALID_CONFIG_FILE: denotes that one or more config files passed to
 *                             the driver is invalid and the driver.
 *        CANNOT_GET_AUDIO_CONFIGURATION : denotes that the driver was not able
 *                             to retrieve the audio configuration.
 */
enum class ErrorCode : int
{
    DEVICE_INACTIVE = 140,
    INVALID_FIRMWARE_VER,
    INVALID_BUFFER_SIZE,
    INVALID_CONFIG_FILE,
    CANNOT_GET_AUDIO_CONFIGURATION
};

/**
 * @brief Struct that represents info about a channel. This info is acquired
 *        from the driver when IOCTLs RASPA_GET_INPUT_CHAN_INFO and
 *        RASPA_GET_OUTPUT_CHAN_INFO is called
 */
struct ChannelInfo {
    uint8_t sw_ch_id;               // The software channel ID or DEVICE_CTRL_AUDIO_CHANNEL_NOT_VALID
    uint8_t hw_ch_id;               // The hardware channel ID or DEVICE_CTRL_AUDIO_CHANNEL_NOT_VALID
    uint8_t direction;              // The audio channel direction as of audio_channel_direction enum
    uint8_t sample_format;          // The sample format as of audio_sample_format enum
    uint8_t channel_name[32];       // The channel name must be a valid null terminated string
    uint32_t start_offset_in_words; // Audio channel data start offset in words
    uint32_t stride_in_words;       // Audio channel data stride in words
};

/**
 * @brief Enumeration of USB audio implementations that the driver informs RASPA
 *        NONE : No usb audio
 *        NATIVE_ALSA : USB audio is performed by RASPA using alsa subsystem
 *        EXTERNAL_UC : External microcontroller handles USB audio. RASPA does
 *        not have to do anything for this kind of types
 */
enum class UsbAudioType : int
{
    NONE = 1,
    NATIVE_ALSA,
    EXTERNAL_UC
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

/**
 * @brief Get the sample rate info from the driver
 *
 * @return int The sample rate
 */
int get_sample_rate()
{
    return read_driver_param(SAMPLE_RATE_PARAM);
}

/**
 * @brief Get info about the number of input channels from the driver
 *
 * @return int The number of input channels
 */
int get_num_input_chan()
{
    return read_driver_param(NUM_INPUT_CHANS_PARAM);
}

/**
 * @brief Get info about the number of output channels from the driver
 *
 * @return int The number of output channels
 */
int get_num_output_chan()
{
    return read_driver_param(NUM_OUTPUT_CHANS_PARAM);
}

/**
 * @brief Get info about the platform type from the driver.
 *
 * @return int one of PlatformType
 */
int get_platform_type()
{
    return read_driver_param(PLATFORM_TYPE_PARAM);
}

/**
 * @brief Get info about the buffer size from the driver.
 *
 * @return int one of PlatformType
 */
int get_buffer_size()
{
    return read_driver_param(BUFFER_SIZE_PARAM);
}

/**
 * @brief Get the usb audio type param from the driver
 *
 * @return int one of UsbAudioType
 */
int get_usb_audio_type()
{
    return read_driver_param(USB_AUDIO_TYPE_PARAM);
}

/**
 * @brief Get the audio irq affinity param from the driver
 *
 * @return int (cpu num)
 */
int get_audio_irq_affinity()
{
    return read_driver_param(IRQ_AFFINITY);
}

/**
 * @brief Check if codec format given by driver is correct
 *
 * @param codec_format the codec format in int as given by driver
 * @return std::pair<bool, CodecFormat> true and CodecFormat if codec_format is valid
 *                                      false and CodecFormat::NONE if invalid
 */
std::pair<bool, CodecFormat> check_codec_format(int codec_format)
{
    if (codec_format <= static_cast<int>(CodecFormat::NONE) ||
        codec_format >= static_cast<int>(CodecFormat::NUM_CODEC_FORMATS))
    {
        return {false, CodecFormat::NONE};
    }

    return {true, static_cast<CodecFormat>(codec_format)};
}

/**
 * @brief Check the driver version.
 *
 * @return std::pair<bool, int> false if version mismatches along with the
           mismatched version, true upon success
 */
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
