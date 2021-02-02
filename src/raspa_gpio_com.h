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
 * @brief Definition of class RaspaGpioCom which handles the process of
 *        exchanging GPIO data with SENSEI using Unix Sockets
 * @copyright 2017-2020 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef RASPA_GPIO_SOCKET_H_
#define RASPA_GPIO_SOCKET_H_

#include <sys/un.h>
#include <sys/socket.h>
#include <utility>
#include <thread>
#include <chrono>

#include <fifo/circularfifo_memory_relaxed_aquire_release.h>

#include "audio_control_protocol/audio_control_protocol.h"

namespace {
    // Address of raspa socket
    constexpr char RASPA_SOCKET[] = "/tmp/raspa";

    // Size of the fifos in between the real time thread and non real time.
    constexpr size_t GPIO_PACKET_Q_SIZE = 100;

    // Blocking timeout period of the socket
    constexpr std::chrono::microseconds SOCKET_TIMEOUT(250000);

    // periodicity of the write thread
    constexpr std::chrono::milliseconds WRITE_LOOP_PERIOD(100);

    // Timeout period to wait for the rt thread to consume an element from a fifo.
    constexpr std::chrono::milliseconds WAIT_FOR_RT_FIFO_TIMEOUT(10);
}

namespace raspa {

using namespace memory_relaxed_aquire_release;

/**
 * Class which is responsible for the tx/rx of GPIO data from
 * the real time thread and transfering that data over UNIX sockets in a real
 * time safe way. It creates two non real time threads which send and receive
 * gpio data from unix sockets and transfer them over to the real time thread,
 */
class RaspaGpioCom
{
public:
    RaspaGpioCom(const std::string& gpio_host_socket_name,
                 RaspaErrorCode* raspa_error_code) :
                                  _in_socket(0),
                                  _out_socket(0),
                                  _gpio_host_socket_name(gpio_host_socket_name),
                                  _is_running(false),
                                  _raspa_error_code(raspa_error_code)
    {}

    ~RaspaGpioCom()
    {
        deinit();
    }

    /**
     * @brief Initialize the GPIO com and all of its internals i.e the queues,
     *        sockets and the non real time communication thread.
     *
     * @return int RASPA_SUCCESS on success, different error code otherwise
     */
    int init()
    {
        _in_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (_in_socket < 0)
        {
            _raspa_error_code->set_error_val(RASPA_EINSOCKET_CREATION,
                                             _in_socket);
            return -RASPA_EINSOCKET_CREATION;
        }

        _out_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (_out_socket < 0)
        {
            _raspa_error_code->set_error_val(RASPA_EOUTSOCKET_CREATION,
                                             _out_socket);
            return -RASPA_EOUTSOCKET_CREATION;
        }

        sockaddr_un address;
        address.sun_family = AF_UNIX;
        strcpy(address.sun_path, RASPA_SOCKET);

        // Clear the socket in case raspa crashed the previous time
        unlink(RASPA_SOCKET);

        // bind the address to in socket
        auto res = bind(_in_socket, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_un));
        if (res < 0)
        {
            _raspa_error_code->set_error_val(RASPA_EINSOCKET_BIND, res);
            return -RASPA_EINSOCKET_BIND;
        }

        // set time out
        timeval time;
        time.tv_sec = 0;
        time.tv_usec = SOCKET_TIMEOUT.count();
        res = setsockopt(_in_socket, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));
        if (res != 0)
        {
            _raspa_error_code->set_error_val(RASPA_EINSOCKET_TIMEOUT, res);
            return -RASPA_EINSOCKET_TIMEOUT;
        }

        // attempt to connect
        _connect_to_gpio_host_socket();
        _is_running = true;

        // setup threads
        _read_thread = std::thread(&RaspaGpioCom::_read_from_socket_loop, this);
        _write_thread = std::thread(&RaspaGpioCom::_write_to_socket_loop, this);

        return RASPA_SUCCESS;
    }

    /**
     * @brief Deinitialize the GPIO Com task.
     */
    void deinit()
    {
        if (!_is_running)
        {
            return;
        }

        _is_running = false;
        if (_read_thread.joinable())
        {
            _read_thread.join();
        }
        if (_write_thread.joinable())
        {
            _write_thread.join();
        }

        unlink(RASPA_SOCKET);
    }

    // Functions that can be called from RT context

    /**
     * @brief Function called by a real time thread to send gpio data.
     *
     * @param gpio_data data blob containing the gpio data
     * @return true upon success, false otherwise.
     */
    bool send_gpio_data_to_nrt(const struct audio_ctrl::GpioDataBlob& gpio_data)
    {
        return _from_rt_gpio_data_fifo.push(gpio_data);
    }

    /**
     * @brief Function called by a real time thread to rx gpio data.
     *
     * @param gpio_data data blob to store the rx gpio data
     * @return true upon success. False when there is no rx data.
     */
    bool get_gpio_data_from_nrt(struct audio_ctrl::GpioDataBlob& gpio_data)
    {
        return _to_rt_gpio_data_fifo.pop(gpio_data);
    }

    /**
     * @brief Check if there is any rx gpio data available to be sent
     *
     * @return true when there is data, false otherwise
     */
    bool rx_gpio_data_available()
    {
        return !_to_rt_gpio_data_fifo.wasEmpty();
    }

private:

    /**
     * @brief Helper function to send gpio data over unix socket
     *
     * @param gpio_data The gpio data blob
     * @return true If successful. False otherwise
     */
    bool _send_gpio_data_to_socket(const struct
                                   audio_ctrl::GpioDataBlob& gpio_data)
    {
        auto bytes = send(_out_socket,
                          &gpio_data,
                          AUDIO_CTRL_PKT_GPIO_DATA_BLOB_SIZE,
                          0);
        if (bytes < static_cast<ssize_t>(AUDIO_CTRL_PKT_GPIO_DATA_BLOB_SIZE))
        {
            return false;
        }

        return true;
    }

    /**
     * @brief Non real time thread that will send gpio data from the fifo
     *        to the unix socket. If data cannot be sent, it tries to checks
     *        socket connection status and reconnects if possible.
     */
    void _write_to_socket_loop()
    {
        struct audio_ctrl::GpioDataBlob gpio_data;
        bool ready_to_send = true;
        auto connected = _connect_to_gpio_host_socket();

        // Try to establish initial connection
        while (_is_running && !connected)
        {
            connected = _connect_to_gpio_host_socket();
            std::this_thread::sleep_for(WRITE_LOOP_PERIOD);
        }

        while (_is_running)
        {
            if (!ready_to_send)
            {
                /* handles condition that prev write was unsuccessful. This can
                   be due to a disconnection. So attempt to connect again.*/
                if (_connect_to_gpio_host_socket())
                {
                    ready_to_send = _send_gpio_data_to_socket(gpio_data);
                }
            }
            else
            {
                while (_from_rt_gpio_data_fifo.pop(gpio_data))
                {
                    ready_to_send = _send_gpio_data_to_socket(gpio_data);
                    if (!ready_to_send)
                    {
                        // TODO : find a way to log this into sushi
                        break;
                    }
                }
            }

            std::this_thread::sleep_for(WRITE_LOOP_PERIOD);
        }
    }

    /**
     * @brief Non real time thread that will read any gpio data received from
     *        the socket and store in into the FIFO for the RT thread to consume
     */
    void _read_from_socket_loop()
    {
        struct audio_ctrl::GpioDataBlob gpio_data;
        bool ready_to_recv = true;

        while (_is_running)
        {
            if (ready_to_recv)
            {
                auto bytes = recv(_in_socket,
                                  &gpio_data,
                                  AUDIO_CTRL_PKT_GPIO_DATA_BLOB_SIZE,
                                  0);
                if (bytes ==
                   static_cast<ssize_t>(AUDIO_CTRL_PKT_GPIO_DATA_BLOB_SIZE))
                {
                    ready_to_recv = _to_rt_gpio_data_fifo.push(gpio_data);
                }
            }
            else
            {

              /* handles situation where previously rcvd gpio data was not
              inserted in the fifo. A sleep is introduced so that the rt thread
              consumes elements from this fifo. Such a situation can be a
              rare occurrence as the RT thread is always faster.*/

                std::this_thread::sleep_for(WAIT_FOR_RT_FIFO_TIMEOUT);
                ready_to_recv = _to_rt_gpio_data_fifo.push(gpio_data);
            }
        }
    }

    /**
     * @brief Helper function to connect to the GPIO host socket in order to
     *        exchange data.
     *
     * @return true In the case of successful connection, false if not
     */
    bool _connect_to_gpio_host_socket()
    {
        sockaddr_un address;
        address.sun_family = AF_UNIX;
        strcpy(address.sun_path, _gpio_host_socket_name.c_str());
        auto res = connect(_out_socket, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_un));
        if (res != 0)
        {
            // TODO : find a way to inform sushi to log this
            return false;
        }

        timeval time;
        time.tv_sec = 0;
        time.tv_usec = SOCKET_TIMEOUT.count();
        res = setsockopt(_out_socket, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof(time));
        if (res != 0)
        {
            // TODO : find a way to inform sushi to log this
            return false;
        }

        return true;
    }

    int _in_socket;
    int _out_socket;

    std::string _gpio_host_socket_name;

    bool _is_running;

    CircularFifo<struct audio_ctrl::GpioDataBlob, GPIO_PACKET_Q_SIZE> _to_rt_gpio_data_fifo;
    CircularFifo<struct audio_ctrl::GpioDataBlob, GPIO_PACKET_Q_SIZE> _from_rt_gpio_data_fifo;

    std::thread _write_thread;
    std::thread _read_thread;

    RaspaErrorCode* _raspa_error_code;
};

}

#endif //RASPA_GPIO_SOCKET_H_
