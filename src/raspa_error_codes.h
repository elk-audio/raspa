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
 * @brief Definition of all Raspa Error codes and helper macros to aid in error
 *        msgs
 * @copyright 2017-2021 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */
#ifndef RASPA_RASPA_ERROR_CODES_H
#define RASPA_RASPA_ERROR_CODES_H

#include <string>
#include <cstring>
#include <cstdlib>
#include <map>

namespace raspa {

// Additional message for parameter related errors
constexpr char DRIVER_PARAM_ERROR_INFO[] = "The driver might not have been"
                              " loaded or has invalid configuration or version.";

/**
 * @brief Macro which will expand macro X on all possible error codes.
 *        X should take 3 arguments
 */
#define ERROR_CODES_OP(X)\
    X(0,   RASPA_SUCCESS, "Raspa: No error. ")\
    X(100, RASPA_EBUFFER_SIZE_MISMATCH, "Raspa: Buffer size mismatch with driver ")\
    X(101, RASPA_EVERSION, "Raspa: Version mismatch with driver ")\
    X(102, RASPA_ENOMEM, "Raspa: Failed to get buffers from driver ")\
    X(103, RASPA_EUSER_BUFFERS, "Raspa: Failed to allocate user audio buffers ")\
    X(104, RASPA_ETASK_AFFINITY, "Raspa: Failed to set affinity for RT task ")\
    X(105, RASPA_ETASK_CREATE, "Raspa: Failed to create RT task ")\
    X(106, RASPA_ETASK_START, "Raspa: Failed to start RT task ")\
    X(107, RASPA_ETASK_STOP, "Raspa: Failed to stop RT task ")\
    X(108, RASPA_ETASK_CANCEL, "Raspa: Failed to cancel RT task ")\
    X(109, RASPA_EUNMAP, "Raspa: Failed to unmap driver buffers ")\
    X(110, RASPA_EDEVICE_OPEN, "Raspa: Failed to open driver ")\
    X(111, RASPA_EDEVICE_CLOSE, "Raspa: Failed to close driver ")\
    X(112, RASPA_ECODEC_FORMAT, "Raspa: Unsupported codec format ")\
    X(113, RASPA_EPLATFORM_TYPE, "Raspa: Unsupported platform type ")\
    X(114, RASPA_EDEVICE_FIRMWARE, "Raspa: Incorrect firmware on external micro-controller ")\
    X(115, RASPA_EDEVICE_INACTIVE, "Raspa: External micro-controller not responding ")\
    X(116, RASPA_EINSOCKET_CREATION, "Raspa: Failed to create input socket for gpio data communication ")\
    X(117, RASPA_EOUTSOCKET_CREATION, "Raspa: Failed to create output socket for gpio data communication ")\
    X(118, RASPA_EINSOCKET_BIND, "Raspa: Failed to bind input socket to address ")\
    X(119, RASPA_EINSOCKET_TIMEOUT, "Raspa: Failed to set input socket to address ")\
    X(120, RASPA_EMLOCKALL, "Raspa: Failed to lock memory needed to prevent page swapping ")\
    X(121, RASPA_EBUFFER_SIZE_INVALID, "Raspa: driver configured with invalid buffer size. ")\
    X(122, RASPA_EBUFFER_SIZE_SC, "Raspa: sample converter does not suppot specified buffer size. ")\
    X(200, RASPA_EPARAM, "Raspa: Unable to param from driver ")\
    X(201, RASPA_EPARAM_SAMPLERATE, "Raspa: Unable to read sample rate param from driver ")\
    X(202, RASPA_EPARAM_INPUTCHANS, "Raspa: Unable to read num input chans param from driver ")\
    X(203, RASPA_EPARAM_OUTPUTCHANS, "Raspa: Unable to read num output chans param from driver ")\
    X(204, RASPA_EPARAM_CODEC_FORMAT, "Raspa: Unable to read codec format param from driver ")\
    X(205, RASPA_EPARAM_PLATFORM_TYPE, "Raspa: Unable to read platform type param from driver ")\
    X(206, RASPA_EPARAM_VERSION, "Raspa: Unable to read driver version param from driver ")\
    X(207, RASPA_EPARAM_BUFFER_SIZE, "Raspa: Unable to access buffer size param of driver ")\
    X(208, RASPA_EALSA_INIT_FAILED, "Raspa: Alsa usb init failed ")\

/**
 * @brief Macro to define the error codes as enums
 */
#define ERROR_ENUM(ID, NAME, TEXT) NAME = ID,

/**
 * @brief Macro to populate the std::map _error_text with error codes and its
 *       corresponding error text.
 */
#define ERROR_TEXT_MAP(ID, NAME, TEXT) _error_text[ID] = TEXT;

/**
 * @brief Macro to populate the std::map _error_val with error codes and its
 *       corresponding linux error codes.
 */
#define ERROR_VAL_MAP(ID, NAME, TEXT) _error_val[ID] = 0;

/**
 * Declarations
 */
enum
{
    ERROR_CODES_OP(ERROR_ENUM)
};

/**
 * Helper class to handle error codes for raspa. Provides functions to store
 * the linux error code associated with a raspa error code and get error message
 * text associated with a raspa error code.
 */
class RaspaErrorCode
{
public:
    RaspaErrorCode()
    {
        ERROR_CODES_OP(ERROR_TEXT_MAP)
        ERROR_CODES_OP(ERROR_VAL_MAP)
    }

    /**
     * @brief Set the corresponding linux error code associated with a
     *        raspa error code.
     * @param raspa_error_code The raspa error code.
     * @param error_val The linux error code
     */
    void set_error_val(int raspa_error_code, int error_val)
    {
        _error_val[raspa_error_code] = std::abs(error_val);
    }

    /**
     * @brief Returns the error text associated with a raspa error code. If
     *        the associated linux error code is non-zero, then the text also
     *        contains information about the linux error code.
     * @param raspa_error_code The raspa error code
     * @return Error text
     */
    const char* get_error_text(int raspa_error_code)
    {
        int _raspa_error_code = std::abs(raspa_error_code);

        auto error_text = _error_text.find(_raspa_error_code);
        if (error_text == _error_text.end())
        {
            return "Raspa: Unknown error";
        }

        auto error_val = _error_val[_raspa_error_code];
        if (error_val == 0)
        {
            return error_text->second;
        }

        // append linux error code info in brackets to the error message
        _error_string = error_text->second;
        _error_string += "(";
        _error_string.append(std::strerror(error_val));
        _error_string += "). ";

        // For parameter related error codes, more info is needed.
        if (_raspa_error_code >= RASPA_EPARAM)
        {
            _error_string.append(DRIVER_PARAM_ERROR_INFO);
        }

        return _error_string.data();
    }

private:
    // Map of error codes and error text
    std::map<int, const char*> _error_text;

    // Map of error codes and error values
    std::map<int, int> _error_val;

    // Error text container
    std::string _error_string;
};

}  // namespace raspa

#endif  // RASPA_RASPA_ERROR_CODES_H
