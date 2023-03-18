#ifndef SIGNAL_RECORDER_H_
#define SIGNAL_RECORDER_H_

#include <sndfile.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <cmath>

constexpr int NUM_INTERRUPTS_TO_IGNORE = 1000;
typedef std::vector<std::vector<float>> RecordingBuffer;

class SignalRecorder
{
public:
    SignalRecorder() : _output_sample_counter(0),
                       _input_buffer_counter(0),
                       _interrupt_counter(0),
                       _done_recording(false),
                       _num_frames(0),
                       _num_input_chans(0),
                       _num_output_chans(0),
                       _sampling_freq(0)
    {}

    void init(std::vector<int> input_chan_list,
              std::vector<int> output_chan_list,
              int output_freq,
              float amplitude,
              int num_buffers_to_record,
              int num_input_chans,
              int num_output_chans,
              int sampling_freq,
              int num_frames,
              std::string recording_path)
    {
        _input_chan_list = input_chan_list;
        _output_chan_list = output_chan_list;
        _output_freq = output_freq;
        _amplitude = amplitude;
        _num_buffers_to_record = num_buffers_to_record;
        _num_input_chans = num_input_chans;
        _num_output_chans = num_output_chans;
        _sampling_freq = sampling_freq;
        _num_frames = num_frames;
        _recording_path = recording_path;

        // std::cout << output_freq << std::endl;
        // std::cout << amplitude << std::endl;
        // std::cout << num_buffers_to_record << std::endl;
        // std::cout << num_input_chans << std::endl;
        // std::cout << num_output_chans << std::endl;
        // std::cout << sampling_freq << std::endl;
        // std::cout << num_frames << std::endl;

        // alloc recording buffer
        _recording_files.resize(_num_input_chans);
        _recording_buffer.resize(_num_input_chans);
        int num_samples_to_record = _num_buffers_to_record * _num_frames;
        for(auto chan : _input_chan_list)
        {
            _recording_buffer[chan].resize(num_samples_to_record, 0);
        }
    }

    bool get_done_recording()
    {
        return _done_recording;
    }

    void write_recording_to_files()
    {
        std::memset(&_soundfile_info, 0, sizeof(_soundfile_info));
        _soundfile_info.samplerate = _sampling_freq;
        _soundfile_info.frames = _num_buffers_to_record * _num_frames;
        _soundfile_info.channels = 1;
        _soundfile_info.format = (SF_FORMAT_WAV | SF_FORMAT_FLOAT);

        // Open the wav files
        std::string file_name_prefix = "chan_";
        std::string file_name_suffix = "_recording.wav";

        for(auto& chan : _input_chan_list)
        {
            std::string file_name = _recording_path + "/" + file_name_prefix + std::to_string(chan)
                                    + file_name_suffix;
            _recording_files[chan] = sf_open(file_name.c_str(), SFM_WRITE, &_soundfile_info);
            if (!_recording_files[chan])
            {
                std::cout << "Unable to open file for chan " << chan << std::endl;
            }
            else
            {
                sf_write_float(_recording_files[chan],_recording_buffer[chan].data(), _recording_buffer[chan].size());
                sf_write_sync(_recording_files[chan]);
                sf_close(_recording_files[chan]);
            }
        }
    }

    static void rt_process_callback(float* input, float* output, void* data)
    {
        return static_cast<SignalRecorder*>(data)->_internal_process_callback(
                input, output);
    }

private:
    void _internal_process_callback(float* input, float* output)
    {
        _generate_output(output);
        _record_inputs(input);
    }

    void _generate_output(float* output)
    {
        for (int i = 0; i < _num_frames; i++)
        {
            float output_sample = _amplitude * sin(2 * M_PI * _output_sample_counter *
                                                (float)_output_freq / (float)_sampling_freq);

            _output_sample_counter++;
            if (_output_sample_counter == (int)_sampling_freq)
            {
                _output_sample_counter = 0;
            }

            for (auto chan : _output_chan_list)
            {
                output[i + (chan * _num_frames)] = output_sample;
            }
        }
    }

    void _record_inputs(float* input)
    {
        if (_interrupt_counter < NUM_INTERRUPTS_TO_IGNORE)
        {
            _interrupt_counter++;
            return;
        }

        if (_input_buffer_counter >= _num_buffers_to_record)
        {
            return;
        }

        int sample_index = _input_buffer_counter * _num_frames;
        for (int i = 0; i < _num_frames; i++)
        {
            for(auto chan : _input_chan_list)
            {
                _recording_buffer[chan][sample_index] = input[i + (chan * _num_frames)];
            }
            sample_index++;
        }

        _input_buffer_counter++;
        if (_input_buffer_counter == _num_buffers_to_record)
        {
            _done_recording = true;
        }
    }

    std::vector<int> _input_chan_list;
    std::vector<int> _output_chan_list;
    int _output_freq;
    float _amplitude;
    int _num_buffers_to_record;

    int _output_sample_counter;
    int _input_buffer_counter;
    int _interrupt_counter;
    bool _done_recording;

    int _num_frames;
    int _num_input_chans;
    int _num_output_chans;
    int _sampling_freq;


    RecordingBuffer _recording_buffer;

    std::vector<SNDFILE*> _recording_files;
    SF_INFO             _soundfile_info;

    std::string _recording_path;
};

#endif //SIGNAL_RECORDER_H_
