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
    X(200, RASPA_EPARAM_BUFFSIZE, "Raspa: Unable to read buffer size param from driver. ")\
    X(201, RASPA_EPARAM_VERSION, "Raspa: Unable to read version param from driver. ")\
    X(202, RASPA_EPARAM_FS, "Raspa: Unable to read sampling freq param from driver. ")\
    X(203, RASPA_EPARAM_INPUTCHANS, "Raspa: Unable to read num input channels param from driver. ")\
    X(204, RASPA_EPARAM_OUTPUTCHANS, "Raspa: Unable to read num output channels param from driver. ")\
    X(205, RASPA_EPARAM_CODECFORMAT, "Raspa: Unable to read codec format param from driver. ")\

/**
 * @brief Macro to define the error codes as enums
 */
#define ERROR_ENUM(ID, NAME, TEXT) NAME = ID,

/**
 * @brief Macro to declare an integer which will hold the corresponding linux
 *        error code for an individual Raspa error code.
 */
#define ERROR_VAL(ID, NAME, TEXT) int error_val_##NAME = 0;

/**
 * @brief Macro to get the error message text and a text description of its
 *        associated linux error code for each Raspa Error Code.
 */
#define GET_ERROR_TEXT(ID, NAME, TEXT) case ID:           \
    if(error_val_##NAME == 0)                             \
    {                                                     \
        return TEXT;                                      \
    }                                                     \
    error_string = TEXT;                                  \
    error_string.append(std::strerror(error_val_##NAME)); \
    return error_string.data();                           \

/**
 * @brief Macro to store the linux error code for a particular Raspa Error code
 *        and return.
 */
#define SET_ERROR_VAL_AND_RET_CODE(NAME, ERROR_VAL) error_val_##NAME = abs(ERROR_VAL); \
    return -NAME;

/**
 * Declarations
 */
enum
{
    ERROR_CODES_OP(ERROR_ENUM)
};

// String which will contain the error message
std::string error_string;

// Declare the integers to store the linux error code.
ERROR_CODES_OP(ERROR_VAL);

} // namespace raspa

#endif //RASPA_ERROR_CODES_H_