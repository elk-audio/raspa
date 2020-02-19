/*
 * Copyright 2018-2019 Modern Ancient Instruments Networked AB, dba Elk
 */

/**
 * com_task.c
 *
 * @brief A task which helps raspa communicate and send msgs to sensei over unix
 *        sockets
 * @copyright 2017-2019 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "com_task.h"
#include "audio_control_protocol/audio_control_protocol.h"

static const char SENSEI_SOCKET[] = "/tmp/sensei";
static const char RASPA_SOCKET[] = "/tmp/raspa";
static const int  SOCKET_TIMEOUT_US = 250000;
static const int  SILENCE_THRESHOLD = 20;
static const int  READ_THREAD_TICK_PERIOD_US = 2000;

// TODO - this is probably too much
#define MAX_PACKETS_IN_QUEUE 100

// The control packet internals are opaque to raspalib
typedef struct RaspaPacket
{
    char data[AUDIO_CONTROL_PACKET_PAYLOAD_SIZE];
} RaspaPacket;

typedef struct CommunicationTaskInstance
{
    CFifo            _to_task_fifo;
    CFifo            _from_task_fifo;
    pthread_t        _read_thread;
    pthread_t        _write_thread;
    bool             _running;
    int              _in_socket;
    int              _out_socket;
    int              _silence_count;
    bool             _connected;
} CommunicationTaskInstance;

int  setup_sockets(CommunicationTaskInstance* instance);

bool connect_to_sensei(CommunicationTaskInstance* instance);

void* read_worker(void*arg);

void* write_worker(void*arg);


int setup_sockets(CommunicationTaskInstance* instance)
{
    instance->_in_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (instance->_in_socket == -1)
    {
        return errno;
    }
    instance->_out_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (instance->_out_socket == -1)
    {
        return errno;
    }

    struct sockaddr_un address;
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, RASPA_SOCKET);

    // In case we didn't quit gracefully, clear the socket handle
    unlink(RASPA_SOCKET);
    int res = bind(instance->_in_socket, (struct sockaddr*)&address, sizeof(struct sockaddr_un));
    if (res != 0)
    {
        return res;
    }

    struct timeval time;
    time.tv_sec = 0;
    time.tv_usec = SOCKET_TIMEOUT_US;
    res = setsockopt(instance->_in_socket, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));
    if (res != 0)
    {
        return res;
    }

    /* Try to connect to sensei. If this fails we still return OK, The connection will be
     * retried when we receive something on the socket we've bound to RASPA_SOCKET */
    instance->_connected = connect_to_sensei(instance);
    return 0;
}

bool connect_to_sensei(CommunicationTaskInstance* instance)
{
    struct sockaddr_un address;
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SENSEI_SOCKET);

    int res = connect(instance->_out_socket, (struct sockaddr*)&address, sizeof(struct sockaddr_un));
    if (res != 0)
    {
        return false;
    }

    struct timeval time;
    time.tv_sec = 0;
    time.tv_usec = SOCKET_TIMEOUT_US;
    res = setsockopt(instance->_out_socket, SOL_SOCKET, SO_SNDTIMEO, &time, sizeof(time));
    if (res != 0)
    {
        return false;
    }

    RaspaPacket empty_packet = {0};
    send(instance->_out_socket, &empty_packet, sizeof(empty_packet), 0);
    return true;
}

void* read_worker(void*arg)
{
    CommunicationTaskInstance* instance = (CommunicationTaskInstance*)arg;
    RaspaPacket buffer;
    while (instance->_running)
    {
        while (c_fifo_receive(instance->_to_task_fifo, (void*)&buffer, sizeof(buffer)) > 0 && instance->_connected)
        {
            send(instance->_out_socket, &buffer, sizeof(buffer), 0);
        }
        usleep(READ_THREAD_TICK_PERIOD_US);
    }
    return NULL;
}

void* write_worker(void*arg)
{
    CommunicationTaskInstance* instance = (CommunicationTaskInstance*)arg;
    RaspaPacket buffer;
    while (instance->_running)
    {
        ssize_t bytes = recv(instance->_in_socket, &buffer, sizeof(buffer), 0);
        if (bytes == sizeof(RaspaPacket))
        {
            c_fifo_send(instance->_from_task_fifo, (void*)&buffer, sizeof(buffer));
            // TODO - Will drop packets if queue becomes full, is that acceptable?
            if (!instance->_connected)
            {
                instance->_connected = connect_to_sensei(instance);
            }
            instance->_silence_count = 0;
        }
        else
        {
            if (++instance->_silence_count > SILENCE_THRESHOLD)
            {
                // If we havent received anything from sensei for a long time, recheck the connection
                instance->_connected = connect_to_sensei(instance);
                instance->_silence_count = 0;
            }
        }
    }
    return NULL;
}

ComTask communication_task_create()
{
    CommunicationTaskInstance* instance = malloc(sizeof(CommunicationTaskInstance));
    if (instance == NULL)
    {
        return NULL;
    }
    memset(instance, 0, sizeof(CommunicationTaskInstance));
    instance->_to_task_fifo = c_fifo_open(sizeof(RaspaPacket), MAX_PACKETS_IN_QUEUE);
    instance->_from_task_fifo = c_fifo_open(sizeof(RaspaPacket), MAX_PACKETS_IN_QUEUE);

    int res = setup_sockets(instance);
    if (res != 0)
    {
        goto ERROR_EXIT;
    }
    instance->_running = true;
    res = pthread_create(&instance->_read_thread, NULL, read_worker, instance);
    if (res != 0)
    {
        instance->_read_thread = 0;
        goto ERROR_EXIT;
    }
    res = pthread_create(&instance->_write_thread, NULL, write_worker, instance);
    if (res != 0)
    {
        instance->_write_thread = 0;
        goto ERROR_EXIT;
    }
    return instance;

ERROR_EXIT:
    if (instance->_read_thread)
    {
        pthread_join(instance->_read_thread, NULL);
    }
    if (instance->_write_thread)
    {
        pthread_join(instance->_write_thread, NULL);
    }
    free(instance);
    return NULL;
}

void communication_task_free(ComTask instance)
{
    if (instance->_running)
    {
        instance->_running = false;
        if (instance->_connected == false)
        {
            /* Post a dummy message to the queue here to wake it up */
            RaspaPacket buffer;
            send(instance->_in_socket, &buffer, sizeof(buffer), 0);
        }
        pthread_join(instance->_read_thread, NULL);
        pthread_join(instance->_write_thread, NULL);
    }
    c_fifo_close(instance->_to_task_fifo);
    c_fifo_close(instance->_from_task_fifo);
    unlink(RASPA_SOCKET);
    free(instance);
}

CFifo input_queue(ComTask instance)
{
    return instance->_to_task_fifo;
}

CFifo output_queue(ComTask instance)
{
    return instance->_from_task_fifo;
}