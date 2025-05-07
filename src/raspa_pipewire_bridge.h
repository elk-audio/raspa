#ifndef RASPALIB_RASPA_PIPEWIRE_BRIDGE_H
#define RASPALIB_RASPA_PIPEWIRE_BRIDGE_H

#ifdef RASPA_WITH_PIPEWIRE

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

namespace raspa{

class RaspaPipewireBridge{
public:
    RaspaPipewireBridge();
    ~RaspaPipewireBridge();

    bool init(float sample_rate, int engine_buffer_size, int num_input_channels, int num_output_channels);

    bool start_streams();

    // Copy incoming data from PW according to the set buffer size and channel count, returns false if not available
    void copy_to_input_buffer(float* buffer, bool zero_if_unavailable);

    // Send a buffer with data to PW according to the set buffer size and channel count, returns false on error
    void put_output_buffer(const float* buffer);

    bool is_thread_running();

    void close();

    static void input_stream_callback(void* instance)
    {
        reinterpret_cast<RaspaPipewireBridge*>(instance)->_input_stream_callback();
    }

    static void output_stream_callback(void* instance)
    {
        reinterpret_cast<RaspaPipewireBridge*>(instance)->_output_stream_callback();
    }

    static void input_stream_state_callback(void* instance, pw_stream_state old, pw_stream_state state, const char *error)
    {
        reinterpret_cast<RaspaPipewireBridge*>(instance)->_input_stream_state_callback(old, state, error);
    }

    static void output_stream_state_callback(void* instance, pw_stream_state old, pw_stream_state state, const char *error)
    {
        reinterpret_cast<RaspaPipewireBridge*>(instance)->_output_stream_state_callback(old, state, error);
    }

private:
    enum class StreamDirection
    {
        IN,
        OUT
    };

    void _input_stream_callback();

    void _output_stream_callback();

    void _input_stream_state_callback(pw_stream_state old, pw_stream_state state, const char *error);

    void _output_stream_state_callback(pw_stream_state old, pw_stream_state state, const char *error);

    void _init_pipewire();

    void _init_fifos();

    pw_stream* _create_stream(const char* name, int channels, pw_stream_events* events, StreamDirection type);

    pw_thread_loop* _loop{nullptr};
    pw_context*     _context{nullptr};
    pw_core*        _core{nullptr};
    pw_registry*    _registry{nullptr};
    spa_hook        _registry_listener;

    pw_stream*      _input_stream;
    pw_stream*      _output_stream;

    pw_stream_events _input_stream_events;
    pw_stream_events _output_stream_events;

    int _raspa_buffer_size;
    int _input_channels;
    int _output_channels;
    float _sample_rate;

    std::vector<std::unique_ptr<float[]>> _input_fifo_buffers;
    std::vector<std::unique_ptr<float[]>> _output_fifo_buffers;
    std::unique_ptr<float[]> _input_residual;
    std::unique_ptr<float[]> _output_residual;

    bool _flush_output_queue{false};

    int _input_fifo_frame_size;
    int _output_fifo_frame_size;

    int _input_buffer_idx;
    int _output_buffer_idx;
    int _input_residual_idx;
    int _output_residual_idx;

    memory_relaxed_aquire_release::CircularFifo<float*, 200>_input_fifo;
    memory_relaxed_aquire_release::CircularFifo<float*, 200>_output_fifo;
};
}

#else

namespace raspa{

// No-op version for when pw bridge is disabled in the build
class RaspaPipewireBridge{
public:
    RaspaPipewireBridge() = default;
    ~RaspaPipewireBridge() = default;

    bool init(float , int , int , int ) {return false;}

    int start_streams() {return true;}

    // Copy incoming data from PW according to the set buffer size and channel count, returns false if not available
    bool copy_to_input_buffer(float* , bool ) {return true;}

    // Send a buffer with data to PW according to the set buffer size and channel count, returns false on error
    bool put_output_buffer(const float* ) { return true;}

    bool is_thread_running() { return false;}

    void close() {}

};
}
#endif

#endif //RASPALIB_RASPA_PIPEWIRE_BRIDGE_H
