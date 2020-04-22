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
 * @brief Definition of all Raspa Error codes and helper macros to aid in error
 *        msgs
 * @copyright 2017-2020 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */
#ifndef RASPA_ERROR_CODES_H_
#define RASPA_ERROR_CODES_H_

#include <string>
#include <cstring>
#include <cstdlib>
#include <map>

namespace raspa {

/**
 * @brief Macro which will expand macro X on all possible error codes.
 *        X should take 3 arguments
 */
#define ERROR_CODES_OP(X)\
    X(0,   RASPA_SUCCESS, "Raspa: No error. ")\
    X(100, RASPA_EBUFFSIZE, "Raspa: Buffer size mismatch with driver. ")\
    X(101, RASPA_EVERSION, "Raspa: Version mismatch with driver. ")\
    X(102, RASPA_ENOMEM, "Raspa: Failed to get buffers from driver. ")\
    X(103, RASPA_EUSER_BUFFERS, "Raspa: Failed to allocate user audio buffers. ")\
    X(104, RASPA_ETASK_AFFINITY, "Raspa: Failed to set affinity for RT task. ")\
    X(105, RASPA_ETASK_CREATE, "Raspa: Failed to create RT task. ")\
    X(106, RASPA_ETASK_START, "Raspa: Failed to start RT task. ")\
    X(107, RASPA_ETASK_STOP, "Raspa: Failed to stop RT task. ")\
    X(108, RASPA_ETASK_CANCEL, "Raspa: Failed to cancel RT task. ")\
    X(109, RASPA_EUNMAP, "Raspa: Failed to unmap driver buffers. ")\
    X(110, RASPA_EDEVICE_OPEN, "Raspa: Failed to open driver. ")\
    X(111, RASPA_EDEVICE_CLOSE, "Raspa: Failed to close driver. ")\
    X(112, RASPA_ECODEC_FORMAT, "Raspa: Unsupported codec format. ")\
    X(200, RASPA_EPARAM, "Raspa: Unable to read parameters from driver. Please check driver configuration. ")\

/**
 * @brief Macro to define the error codes as enums
 */
#define ERROR_ENUM(ID, NAME, TEXT) NAME = ID,

#define ERROR_TEXT_MAP(ID, NAME, TEXT) _error_text[ID] = TEXT;
#define ERROR_VAL_MAP(ID, NAME, TEXT) _error_val[ID] = 0;

/**
 * Declarations
 */
enum
{
    ERROR_CODES_OP(ERROR_ENUM)
};

class RaspaErrorCode
{
public:
    RaspaErrorCode()
    {
        ERROR_CODES_OP(ERROR_TEXT_MAP);
        ERROR_CODES_OP(ERROR_VAL_MAP);
    }

    void set_error_val(int raspa_error_code, int error_val)
    {
        _error_val[raspa_error_code] = std::abs(error_val);
    }

    const char* get_error_text(int raspa_error_code)
    {
        int _raspa_error_code = std::abs(raspa_error_code);

        auto error_text = _error_text.find(_raspa_error_code);
        if(error_text == _error_text.end())
        {
            return "Raspa: Unknown error";
        }

        auto error_val = _error_val[_raspa_error_code];
        if(error_val == 0)
        {
            return error_text->second;
        }

        _error_string = error_text->second;
        _error_string.append(std::strerror(error_val));
        return _error_string.data();
    }

private:
    std::map<int, const char*> _error_text;
    std::map<int, int> _error_val;
    std::string _error_string;
};

} // namespace raspa

#endif //RASPA_ERROR_CODES_H_