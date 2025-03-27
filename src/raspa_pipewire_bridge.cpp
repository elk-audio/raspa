#include <iostream>
#include <memory>
#include <vector>
#include <cassert>

#include <pipewire/impl.h>
#include <pipewire/i18n.h>
#include <pipewire/stream.h>
#include <spa/param/audio/format-utils.h>

#include "fifo/circularfifo_memory_relaxed_aquire_release.h"

#include "raspa_pipewire_bridge.h"
#include "sample_conversion.h"

namespace raspa {

constexpr auto INPUT_STREAM_NAME = "Raspa input";
constexpr auto OUTPUT_STREAM_NAME = "Raspa output";
constexpr int AUDIO_BUFFERS = 32;
constexpr int SAMPLE_LATENCY = 1024;

static void registry_event_global(void* data, uint32_t id,
                                  uint32_t permissions, const char* type, uint32_t version,
                                  const struct spa_dict* props)
{
    //printf("object: id:%u type:%s/%d\n", id, type, version);
}

static const pw_registry_events registry_events = {.version = PW_VERSION_REGISTRY_EVENTS,
                                                   .global = registry_event_global,
};

template <typename T, size_t size>
void flush_queue(memory_relaxed_aquire_release::CircularFifo<T, size>& queue, int remainder)
{
    int elements_to_drop = std::min(0, queue.size() - remainder);
    T element;
    for (int i = 0; i < elements_to_drop; ++i)
    {
        queue.pop(element);
    }
}

RaspaPipewireBridge::RaspaPipewireBridge()
{

}

RaspaPipewireBridge::~RaspaPipewireBridge()
{
    close();
}

bool RaspaPipewireBridge::init(float sample_rate,
                               int engine_buffer_size,
                               int num_input_channels,
                               int num_output_channels)
{
    _sample_rate = sample_rate;
    _raspa_buffer_size = engine_buffer_size;
    _input_channels = num_input_channels;
    _output_channels = num_output_channels;
    _init_fifos();
    _init_pipewire();
    return true;
}

bool RaspaPipewireBridge::start_streams()
{
    if (_create_input_stream() && _create_output_stream())
    {
        return true;
    }
    return false;
}

void RaspaPipewireBridge::copy_to_input_buffer(float* buffer, bool zero_if_unavailable)
{
    float* pw_buffer = nullptr;
    if (_input_fifo.pop(pw_buffer))
    {
        assert(pw_buffer);
        std::copy(pw_buffer, pw_buffer + _input_fifo_frame_size, buffer);
    }
    else if (zero_if_unavailable)
    {
        std::fill(buffer, buffer + _raspa_buffer_size, 0.0f);
    }
}

void RaspaPipewireBridge::put_output_buffer(const float* buffer)
{
    auto pw_buffer = _output_fifo_buffers[_output_buffer_idx++].get();
    if (_output_buffer_idx >= (static_cast<int>(_output_fifo_buffers.size())))
    {
        _output_buffer_idx = 0;
    }
    std::copy(buffer, buffer + _output_fifo_frame_size, pw_buffer);
    _output_fifo.push(pw_buffer);
}

bool RaspaPipewireBridge::is_thread_running()
{
    return _loop;
}

void RaspaPipewireBridge::close()
{
    std::cout << "Close called()" << std::endl;
    pw_thread_loop_lock(_loop);
    pw_stream_destroy(_input_stream);
    pw_stream_destroy(_output_stream);
    pw_thread_loop_unlock(_loop);

    if (_loop)
    {
        pw_thread_loop_stop(_loop);
        pw_thread_loop_destroy(_loop);
    }

    pw_proxy_destroy((struct pw_proxy*) _registry);
    pw_core_disconnect(_core);
    pw_context_destroy(_context);
    pw_thread_loop_destroy(_loop);
}

void RaspaPipewireBridge::_input_stream_callback()
{
    pw_buffer *buf = pw_stream_dequeue_buffer(_input_stream);
    if (!buf)
    {
        return;
    }

    static int blocksize = 0;

    spa_data* bd = &buf->buffer->datas[0];

    int offs = SPA_MIN(bd->chunk->offset, bd->maxsize);
    int size = SPA_MIN(bd->chunk->size, bd->maxsize - offs) / sizeof(float);
    auto data = reinterpret_cast<const float*>(SPA_PTROFF(bd->data, offs, void));

    if (size != blocksize)
    {
        //std::cout  << "Input blocksize: " << size << std::endl;
        blocksize = size;
    }

    while (size > 0)
    {
        if (size >= _input_fifo_frame_size)
        {
            auto buffer = _input_fifo_buffers[_input_buffer_idx++].get();
            if (_input_buffer_idx >= static_cast<int>(_input_fifo_buffers.size()))
            {
                _input_buffer_idx = 0;
            }

            if (_input_residual_idx > 0) // if remnants left over from last callback
            {
                int non_residual = _raspa_buffer_size - _input_residual_idx;
                assert(non_residual >= 0);
                copy_interleaved_to_non_interleaved(_input_residual.get(), buffer, _input_channels, _input_residual_idx);
                copy_interleaved_to_non_interleaved(data, buffer + non_residual, _input_channels, non_residual);
                _input_residual_idx = 0;
                size -= non_residual;
                data += non_residual;
            }
            else
            {
                copy_interleaved_to_non_interleaved(data, buffer, _input_channels, _raspa_buffer_size);
                size -= _input_fifo_frame_size;
                data += _input_fifo_frame_size;
            }
            _input_fifo.push(buffer);
        }
        else // residual left
        {
            std::copy(data, data + size, _input_residual.get());
            _input_residual_idx = size;
            size = 0;
        }
    }
    pw_stream_queue_buffer(_input_stream, buf);
}

void RaspaPipewireBridge::_output_stream_callback()
{
    pw_buffer *buf = pw_stream_dequeue_buffer(_output_stream);
    if (!buf)
    {
        //std::cout << "Out of buffers!" <<std::endl;
        return;
    }

    spa_data* bd = &buf->buffer->datas[0];

    int offs = SPA_MIN(bd->chunk->offset, bd->maxsize);
    int size = SPA_MIN(bd->chunk->size, bd->maxsize - offs) / sizeof(float);
    auto data = reinterpret_cast<float*>(SPA_PTROFF(bd->data, offs, void));

    if (_flush_output_queue)
    {
        flush_queue(_output_fifo, 2 * SAMPLE_LATENCY / _raspa_buffer_size);
        _flush_output_queue = false;
    }

    while (size > 0)
    {
        float* raspa_buffer = nullptr;
        if (_output_fifo.pop(raspa_buffer))
        {
            if (size >= _output_fifo_frame_size)
            {
                copy_non_interleaved_to_interleaved(raspa_buffer, data, _output_channels, _output_fifo_frame_size);
                size -= _input_fifo_frame_size;
                data += _input_fifo_frame_size;
            }
            // TODO - Deal with residual later
        }
        else
        {
            std::fill(data, data + size, 0.0f);
        }
    }

    pw_stream_queue_buffer(_input_stream, buf);
}

void RaspaPipewireBridge::_input_stream_state_callback(pw_stream_state old, pw_stream_state state, const char* error)
{
    std::cout << "Input stream changed from " << old << " to " << state  << std::endl;
}

void RaspaPipewireBridge::_output_stream_state_callback(pw_stream_state old, pw_stream_state state, const char* error)
{
    std::cout << "Output stream changed from " << old << " to " << state << std::endl;
    if (state == PW_STREAM_STATE_STREAMING && old != PW_STREAM_STATE_STREAMING)
    {
        _flush_output_queue = true;
    }
}

void RaspaPipewireBridge::_init_pipewire()
{
    std::cout << "Setting up pipewire" << std::endl;
    pw_init(0, nullptr);
    _loop = pw_thread_loop_new(nullptr, nullptr);

    auto properties = pw_properties_new(PW_KEY_APP_NAME, "Raspa Bridge", nullptr);
    _context = pw_context_new(pw_thread_loop_get_loop(_loop), properties, 0 /* user_data size */);
    _core = pw_context_connect(_context, NULL /* properties */, 0 /* user_data size */);

    _registry = pw_core_get_registry(_core, PW_VERSION_REGISTRY, 0 /* user_data size */);

    spa_zero(_registry_listener);
    pw_registry_add_listener(_registry, &_registry_listener, &registry_events, NULL);
    pw_thread_loop_start(_loop);
    std::cout << "Started loop" << std::endl;
}

void RaspaPipewireBridge::_init_fifos()
{
    _input_fifo_frame_size = _raspa_buffer_size * _input_channels;
    _output_fifo_frame_size = _raspa_buffer_size * _output_channels;

    for (int i = 0; i < AUDIO_BUFFERS ; ++i)
    {
        _input_fifo_buffers.push_back(std::make_unique<float[]>(_input_fifo_frame_size));
        _output_fifo_buffers.push_back(std::make_unique<float[]>(_output_fifo_frame_size));
    }
    _input_residual = std::make_unique<float[]>(_raspa_buffer_size * _input_channels);
    _output_residual = std::make_unique<float[]>(_raspa_buffer_size * _output_channels);

    _input_buffer_idx = 0;
    _output_buffer_idx = 0;
    _input_residual_idx = 0;
    _output_residual_idx = 0;
}

bool RaspaPipewireBridge::_create_input_stream()
{
     pw_thread_loop_lock(_loop); // Needed by pw unless we call these functions from the pw thread, which we don't

     auto stream_props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                           PW_KEY_MEDIA_CLASS, "Audio/Sink",
                                           //PW_KEY_MEDIA_CATEGORY, "Playback",
                                           PW_KEY_NODE_NAME, INPUT_STREAM_NAME,
                                           PW_KEY_NODE_VIRTUAL, "true",
                                           PW_KEY_AUDIO_RATE, std::to_string(_sample_rate).c_str(),
                                           PW_KEY_NODE_LATENCY, (std::to_string(SAMPLE_LATENCY) + "/" + std::to_string(_sample_rate)).c_str(),
                                           PW_KEY_AUDIO_CHANNELS, std::to_string(_input_channels).c_str(),
                                           PW_KEY_AUDIO_FORMAT, "F32BE", nullptr);

    _input_stream_events = {.version = PW_VERSION_STREAM_EVENTS,
                            .state_changed = this->input_stream_state_callback,
                            .process = this->input_stream_callback};

    _input_stream = pw_stream_new_simple(pw_thread_loop_get_loop(_loop),
                                         INPUT_STREAM_NAME,
                                         stream_props, &_input_stream_events, this);
    if (!_input_stream)
    {
        pw_thread_loop_unlock(_loop);
        std::cout << "Failed to create output stream" << std::endl;
        return false;
    }

    uint32_t n_params = 1;
    const spa_pod *params[1];
    uint8_t buffer[1024];
    spa_pod_builder b;

    spa_pod_builder_init(&b, buffer, sizeof(buffer));
    spa_audio_info_raw info{.format = SPA_AUDIO_FORMAT_F32,
                            .rate = (uint32_t)_sample_rate,
                            .channels = (uint32_t)_input_channels};
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    int res = pw_stream_connect(_input_stream,
                                PW_DIRECTION_INPUT,
                                PW_ID_ANY,
                                (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                PW_STREAM_FLAG_MAP_BUFFERS |
                                PW_STREAM_FLAG_RT_PROCESS),
                                params, n_params);

    pw_thread_loop_unlock(_loop);
    return res == 0;
}

bool RaspaPipewireBridge::_create_output_stream()
{
    pw_thread_loop_lock(_loop); // Needed by pw unless we call these functions from the pw thread, which we don't

    auto stream_props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                          PW_KEY_MEDIA_CLASS, "Audio/Source",
                                          //PW_KEY_MEDIA_CATEGORY, "Monitor",
                                          PW_KEY_NODE_NAME, OUTPUT_STREAM_NAME,
                                          PW_KEY_NODE_VIRTUAL, "true",
                                          PW_KEY_AUDIO_RATE, std::to_string(_sample_rate).c_str(),
                                          PW_KEY_NODE_LATENCY, (std::to_string(SAMPLE_LATENCY) + "/" + std::to_string(_sample_rate)).c_str(),
                                          PW_KEY_AUDIO_CHANNELS, std::to_string(_output_channels).c_str(),
                                          PW_KEY_AUDIO_FORMAT, "F32BE", nullptr);

    _output_stream_events = {.version = PW_VERSION_STREAM_EVENTS,
                             .state_changed = this->output_stream_state_callback,
                             .process = this->output_stream_callback};

    _output_stream = pw_stream_new_simple(pw_thread_loop_get_loop(_loop),
                                          OUTPUT_STREAM_NAME,
                                          stream_props, &_output_stream_events, this);
    if (!_output_stream)
    {
        pw_thread_loop_unlock(_loop);
        return false;
    }

    uint32_t n_params = 1;
    const spa_pod *params[1];
    uint8_t buffer[1024];
    spa_pod_builder b;

    spa_pod_builder_init(&b, buffer, sizeof(buffer));
    spa_audio_info_raw info{.format = SPA_AUDIO_FORMAT_F32,
                            .rate = (uint32_t)_sample_rate,
                            .channels = (uint32_t)_output_channels};
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    int res = pw_stream_connect(_output_stream,
                                PW_DIRECTION_OUTPUT,
                                PW_ID_ANY,
                                (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                                                  PW_STREAM_FLAG_MAP_BUFFERS |
                                                  PW_STREAM_FLAG_RT_PROCESS),
                                params,
                                n_params);

    pw_thread_loop_unlock(_loop);
    return res == 0;
}

}