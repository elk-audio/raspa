/*
 * Copyright 2022-2025 Elk Audio AB, dba Elk
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
#ifndef RASPA_RUN_LOGGER_H
#define RASPA_RUN_LOGGER_H

#include <unistd.h>
#include <string>
#include <fstream>
#include <thread>

#include "driver_config.h"
#include "raspa/raspa.h"
#include "raspa_error_codes.h"

namespace raspa {

// run logger buffer size
constexpr int PERIOD_LOGGER_BUFFER_SIZE_BITS = 13;
constexpr int PERIOD_LOGGER_BUFFER_SIZE = 0x1 << PERIOD_LOGGER_BUFFER_SIZE_BITS;

// run logger writer thread sleep period (should be small enough depending on buffer size, sample rate and system speed)
constexpr std::chrono::milliseconds PERIOD_LOGGER_WRITER_SLEEP(500);

/**
 * @brief Internal class used by raspa to log the run period data to file.
 */
class RaspaRunLogger
{
public:
    RaspaRunLogger() :
        _is_running(false)
    {
    }

    ~RaspaRunLogger() = default;

    /**
     * @brief Start the run logger. This function allocates all the run logger resources
     *        and creates the run logger thread.
     *
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise.
     */
    int start(std::string &file_name)
    {
        _log_stream.open(file_name.c_str(), std::ofstream::binary | std::ofstream::out);
        if (_log_stream.fail())
        {
            return -RASPA_ERUNLOG_FILE_OPEN;
        }

        _write_count = 0;
        _read_count = 0;
        _overrun = false;
        _is_running = true;
        _thread = std::thread(&RaspaRunLogger::_run, this);

        return RASPA_SUCCESS;
    }

    /**
     * @brief Terminate the run logger. This function stops the run logger thread and
     *        deallocates any resource used by the run logger. It is always safe to
     *        call this function.
     *
     * @return RASPA_SUCCESS upon success, different raspa error code otherwise.
     */
    int terminate()
    {
        if (_is_running)
        {
            _is_running = false;
            if (_thread.joinable())
            {
                _thread.join();
            }
        }

        if (_log_stream.is_open())
        {
            _log_stream.close();
            if (_log_stream.fail())
            {
                return -RASPA_ERUNLOG_FILE_CLOSE;
            }
        }

        return RASPA_SUCCESS;
    }

    /**
     * @brief Put run data into the run logger buffer.
     */
    void put(RaspaMicroSec start, RaspaMicroSec end)
    {
        if (_is_running)
        {
            auto num = _write_count - _read_count;

            if (num < (2*PERIOD_LOGGER_BUFFER_SIZE - 1))
            {
                auto index = _buf_index(_write_count);
                auto offset = _buf_offset(_write_count);

                _buffer[index][offset].start = start;
                _buffer[index][offset].end = end;

                _write_count++;
            }
            else
            {
                _overrun = true;
            }
        }
    }

private:

    void _run()
    {
        while (_is_running)
        {
            std::this_thread::sleep_for(PERIOD_LOGGER_WRITER_SLEEP);
            _write_buffer_to_file(false);
        }

        // write last buffer in case there is any more data pending
        _write_buffer_to_file(true);
        pthread_exit(nullptr);
    }

    void _write_buffer_to_file(bool flush)
    {
        auto threshold = flush ? 1 : PERIOD_LOGGER_BUFFER_SIZE;
        auto count = _write_count - _read_count;

        if (count >= threshold)
        {
            auto index = _buf_index(_read_count);
            auto ptr = &_buffer[index][0];

            // maximum write size is one full buffer
            if (count > PERIOD_LOGGER_BUFFER_SIZE)
            {
                count = PERIOD_LOGGER_BUFFER_SIZE;
            }

            if (_overrun)
            {
                // overrun is stored as 0 timestamps
                _buffer[index][0].start = 0;
                _buffer[index][0].end = 0;
                _overrun = false;
            }
            _log_stream.write(reinterpret_cast<char*>(ptr), count * sizeof(struct run_log_item));
            if (!_log_stream)
            {
                fprintf(stderr, "Logger file write error\n");
            }
            _read_count += count;
        }
    }

    int _buf_index(int count)
    {
        return (count >> PERIOD_LOGGER_BUFFER_SIZE_BITS) & 0x1;
    }

    int _buf_offset(int count)
    {
        return count & ((0x1 << PERIOD_LOGGER_BUFFER_SIZE_BITS) - 1);
    }

    bool _is_running;
    std::thread _thread;
    std::ofstream _log_stream;
    struct run_log_item
    {
        RaspaMicroSec start;
        RaspaMicroSec end;
    } _buffer[2][PERIOD_LOGGER_BUFFER_SIZE];
    std::atomic<int> _write_count;
    std::atomic<int> _read_count;
    std::atomic<bool> _overrun;
};

}  // namespace raspa

#endif  // RASPA_RUN_LOGGER_H
