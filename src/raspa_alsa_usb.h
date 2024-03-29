/*
 * Copyright 2018-2021 Modern Ancient Instruments Networked AB, dba Elk
 */

/**
 * @brief Definition of class RaspaAlsaUsb which handles the process of
 *        USB audio using ALSA interfaces.
 * @copyright 2017-2021 Modern Ancient Instruments Networked AB, dba Elk, Stockholm
 */

#ifndef RASPA_ALSA_USB_H_
#define RASPA_ALSA_USB_H_

#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>

#include <pthread.h>
#include <alsa/asoundlib.h>
#include <fifo/circularfifo_memory_relaxed_aquire_release.h>

#include "driver_config.h"

namespace {
    // Device name of the USB audio gadget listed by ALSA
    constexpr char RASPA_USB_ALSA_DEVICE[] = "hw:0,0";
    //Num of ALSA buffers per each raspa buffer.(ALSA is working in nrt domain and it ll be slower than raspa)
    constexpr int RASPA_TO_ALSA_PERIOD_RATIO = 8;
    // Num of periods per ALSA buffer (Num of times ALSA wakes up per buffer)
    constexpr int ALSA_PERIOD_TO_BUFFER_RATIO = 4;
    // Addional buffer for put and output usb streams
    constexpr int ADDITIONAL_IO_BUFFER_RATIO = 16;
    // Num of raspa buffers fittingin the USB IO buffers
    constexpr int RASPA_TO_USB_IO_BUFFER_RATIO = (RASPA_TO_ALSA_PERIOD_RATIO *
                                    ALSA_PERIOD_TO_BUFFER_RATIO *
                                    ADDITIONAL_IO_BUFFER_RATIO);
}

namespace raspa {

typedef enum
{
    PLAYBACK = 0,
    CAPTURE = 1,
} UsbStream;

//Num of ALSA USB channels is hardcoded to 2 inputs and 2 outputs
constexpr int NUM_ALSA_USB_CHANNELS = 2;

// snd_pcm_format_t is set to SND_PCM_FORMAT_S32_LE which is equivalent to INT32
constexpr snd_pcm_format_t ALSA_USB_SND_PCM_FORMAT = SND_PCM_FORMAT_S32_LE;
constexpr driver_conf::CodecFormat ALSA_USB_CODEC_FORMAT = driver_conf::CodecFormat::INT32;

// check to ensure that pcm format and codec format are not changed
static_assert(ALSA_USB_SND_PCM_FORMAT == SND_PCM_FORMAT_S32_LE &&
              ALSA_USB_CODEC_FORMAT == driver_conf::CodecFormat::INT32,
              " Invalid ALSA pcm format and equivalent codec format");

class RaspaAlsaUsb
{
public:
    RaspaAlsaUsb() = default;
    ~RaspaAlsaUsb() = default;

   int init(int sample_rate, int engine_buffer_size, int num_channels)
   {
        int ret;
        _engine_buffer_size_frames = engine_buffer_size;
        _num_channels = num_channels;
        _sample_rate = sample_rate;
        _alsa_period_size_frames = _engine_buffer_size_frames * RASPA_TO_ALSA_PERIOD_RATIO;
        _alsa_buffer_size_frames = _alsa_period_size_frames * ALSA_PERIOD_TO_BUFFER_RATIO;

        _usb_audio_in_buff = std::make_unique<int32_t[]>(_alsa_buffer_size_frames * _num_channels * ADDITIONAL_IO_BUFFER_RATIO);
        _usb_audio_out_buff = std::make_unique<int32_t[]>(_alsa_buffer_size_frames * _num_channels * ADDITIONAL_IO_BUFFER_RATIO);

        snd_pcm_hw_params_alloca(&_snd_hw_params);
        snd_pcm_sw_params_alloca(&_snd_sw_params);

        snd_output_stdio_attach(&_snd_output, stdout, 0);
        ret = snd_pcm_open(&_pcm_playback_handle, RASPA_USB_ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if (ret)
        {
            return ret;
        }

        ret = snd_pcm_open(&_pcm_capture_handle, RASPA_USB_ALSA_DEVICE, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
        if (ret)
        {
            return ret;
        }

        ret = _set_hw_params(_pcm_capture_handle, SND_PCM_ACCESS_MMAP_INTERLEAVED);
        if (ret)
        {
            return ret;
        }
        ret = _set_hw_params(_pcm_playback_handle, SND_PCM_ACCESS_MMAP_INTERLEAVED);
        if (ret)
        {
            return ret;
        }

        ret = _set_sw_params(_pcm_capture_handle, UsbStream::CAPTURE);
        if (ret)
        {
            return ret;
        }
        ret = _set_sw_params(_pcm_playback_handle, UsbStream::PLAYBACK);
        if (ret)
        {
            return ret;
        }
#ifdef RASPA_DEBUG_PRINT
        snd_pcm_dump(_pcm_capture_handle, _snd_output);
        snd_pcm_dump(_pcm_playback_handle, _snd_output);
#endif
        return 0;
    }

    int start_usb_streams()
    {
        _is_usb_running = true;
        _alsa_worker_start();
        return 0;
    }

    int* get_usb_out_buffer_for_raspa()
    {
        return &_usb_audio_out_buff[_raspa_out_buf_idx * _engine_buffer_size_frames * _num_channels];
    }

    int* get_usb_in_buffer_for_raspa()
    {
        return &_usb_audio_in_buff[_raspa_in_buf_idx * _engine_buffer_size_frames * _num_channels];
    }

    void increment_buf_indices()
    {
        _raspa_out_buf_idx++;
        if (_raspa_out_buf_idx == RASPA_TO_USB_IO_BUFFER_RATIO)
        {
            _raspa_out_buf_idx = 0;
        }
    }

    bool is_usb_thread_running()
    {
        return _is_usb_running;
    }

    int get_usb_input_samples(int32_t*& input_buf)
    {
        return !_input_usb_fifo.pop(input_buf);
    }

    int put_usb_output_samples(int32_t *output_buf)
    {
        return !_output_usb_fifo.push(output_buf);
    }

    int close()
    {
        _is_usb_running = false;
        snd_pcm_close(_pcm_playback_handle);
        snd_pcm_close(_pcm_capture_handle);
        return 0;
    }

private:

    int _set_hw_params(snd_pcm_t *_pcm_handle, snd_pcm_access_t access)
    {
        unsigned int rrate;
        snd_pcm_uframes_t size;
        int err, dir;
        int resample = 0;

        err = snd_pcm_hw_params_any(_pcm_handle, _snd_hw_params);
        if (err < 0)
        {
            return err;
        }

        err = snd_pcm_hw_params_set_rate_resample(_pcm_handle,
                                        _snd_hw_params, resample);
        if (err < 0)
        {
            return err;
        }

        err = snd_pcm_hw_params_set_access(_pcm_handle,
                                            _snd_hw_params, access);
        if (err < 0)
        {
            return err;
        }

        err = snd_pcm_hw_params_set_format(_pcm_handle, _snd_hw_params,
                                            _snd_format);
        if (err < 0)
        {
            return err;
        }
        err = snd_pcm_hw_params_set_channels(_pcm_handle,
                                            _snd_hw_params, _num_channels);
        if (err < 0)
        {
            return err;
        }
        rrate = _sample_rate;
        err = snd_pcm_hw_params_set_rate_near(_pcm_handle,
                                            _snd_hw_params, &rrate, 0);
        if (err < 0)
        {
            return err;
        }
        if (rrate != _sample_rate)
        {
            return -EINVAL;
        }
        err = snd_pcm_hw_params_set_buffer_size_near(_pcm_handle,
                                    _snd_hw_params, &_alsa_buffer_size_frames);
        if (err < 0)
        {
            return err;
        }
        err = snd_pcm_hw_params_get_buffer_size(_snd_hw_params, &size);
        if (err < 0)
        {
            return err;
        }
        _alsa_buffer_size_frames = size;

        err = snd_pcm_hw_params_set_period_size_near(_pcm_handle,
                            _snd_hw_params, &_alsa_period_size_frames, &dir);
        if (err < 0)
        {
            return err;
        }
        err = snd_pcm_hw_params_get_period_size(_snd_hw_params, &size, &dir);
        if (err < 0)
        {
            return err;
        }
        _alsa_period_size_frames = size;
        err = snd_pcm_hw_params(_pcm_handle, _snd_hw_params);
        if (err < 0)
        {
            return err;
        }

        return 0;
    }

    int _set_sw_params(snd_pcm_t *_pcm_handle, UsbStream stream)
    {
        int err;
        snd_pcm_uframes_t start_threshold;
        snd_pcm_uframes_t min_avail = _alsa_period_size_frames;

        if (stream == UsbStream::PLAYBACK)
        {
            start_threshold = _alsa_period_size_frames * (ALSA_PERIOD_TO_BUFFER_RATIO - 1);
        }
        else
        {
            start_threshold = _alsa_period_size_frames * (ALSA_PERIOD_TO_BUFFER_RATIO - 1);
        }

        err = snd_pcm_sw_params_current(_pcm_handle, _snd_sw_params);
        if (err < 0)
        {
            return err;
        }

        err = snd_pcm_sw_params_set_start_threshold(_pcm_handle,
         _snd_sw_params, start_threshold);
        if (err < 0)
        {
            return err;
        }

        err = snd_pcm_sw_params_set_avail_min(_pcm_handle,
         _snd_sw_params, min_avail);
        if (err < 0)
        {
            return err;
        }

        err = snd_pcm_sw_params(_pcm_handle, _snd_sw_params);
        if (err < 0)
        {
            return err;
        }
        return 0;
    }

    int _xrun_recovery(snd_pcm_t *handle, int err)
    {
        if (err == -EPIPE)
        {    /* under-run */
            err = snd_pcm_prepare(handle);
            if (err < 0)
            {
#ifdef RASPA_DEBUG_PRINT
                fprintf(stderr, "Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
#endif
            }
            return 0;
        }
        else if (err == -ESTRPIPE)
        {
            while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            {
                sleep(1);   /* wait until the suspend flag is released */
            }
            if (err < 0)
            {
                err = snd_pcm_prepare(handle);
                if (err < 0)
                {
#ifdef RASPA_DEBUG_PRINT
                    fprintf(stderr, "Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
#endif
                }
            }
            return 0;
        }
        return err;
    }

    int _write_to_pcm(int32_t *frame_ptr, int frames_cntr)
    {
        int frames_written;

        frames_written = snd_pcm_mmap_writei(_pcm_playback_handle,
                                        frame_ptr, frames_cntr);
        if (frames_written == -EAGAIN)
        {
            usleep((1000000/_sample_rate) * _engine_buffer_size_frames);
        }
        else if (frames_written < 0)
        {
            int err = _xrun_recovery(_pcm_playback_handle, frames_written);
            if (err < 0)
            {
                return err;
            }
        }
        return 0;
    }

    void* _usb_output_worker()
    {
        int32_t *frame_ptr;

        while (_is_usb_running)
        {
            if (_output_usb_fifo.pop(frame_ptr))
            {
                if (_write_to_pcm(frame_ptr, _engine_buffer_size_frames))
                {
                    return nullptr;
                }
            }
            else
            {
                usleep((1000000/_sample_rate) * _engine_buffer_size_frames);
            }
        }
        return nullptr;
    }

    int _read_from_pcm(int32_t *frame_ptr, int frames_cntr)
    {
        int frames_read;
        while (frames_cntr > 0)
        {
            frames_read = snd_pcm_mmap_readi(_pcm_capture_handle,
                                            frame_ptr, frames_cntr);
            if (frames_read == -EAGAIN)
            {
                usleep((1000000/_sample_rate) * _engine_buffer_size_frames);
                continue;
            }
            else if (frames_read < 0)
            {
                int err = _xrun_recovery(_pcm_capture_handle, frames_read);
                if (err < 0)
                {
                    return err;
                }
            }
            frame_ptr += frames_read * _num_channels;
            frames_cntr -= frames_read;
        }
        return 0;
    }

    void* _usb_input_worker()
    {
        int *frame_ptr;

        while (_is_usb_running)
        {
            frame_ptr = &_usb_audio_in_buff[_raspa_in_buf_idx *
                            _engine_buffer_size_frames * _num_channels];

            if (_read_from_pcm(frame_ptr, _engine_buffer_size_frames))
            {
                return nullptr;
            }
            if (!_input_usb_fifo.push(frame_ptr))
            {
                usleep((1000000/_sample_rate) * _engine_buffer_size_frames);
            }
            _raspa_in_buf_idx++;
            if (_raspa_in_buf_idx == RASPA_TO_USB_IO_BUFFER_RATIO)
            {
                _raspa_in_buf_idx = 0;
            }
        }
        return nullptr;
    }

    static void* _usb_in_thread(void* data)
    {
        auto raspa_alsa_usb = reinterpret_cast<RaspaAlsaUsb *>(data);
        return raspa_alsa_usb->_usb_input_worker();
    }

    static void* _usb_out_thread(void* data)
    {
        auto raspa_alsa_usb = reinterpret_cast<RaspaAlsaUsb *>(data);
        return raspa_alsa_usb->_usb_output_worker();
    }

    int _alsa_worker_start()
    {
        struct sched_param rt_params = {.sched_priority = 75};
        pthread_attr_t task_attributes;
        int res;

        pthread_attr_init(&task_attributes);

        pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
        pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
        pthread_attr_setschedparam(&task_attributes, &rt_params);

        res = pthread_create(&_usb_in_worker, &task_attributes,
                                &this->_usb_in_thread, this);
        if (res)
        {
            fprintf(stderr, "Error opening file: %s\n", strerror(res));
            return -1;
        }

        res = pthread_create(&_usb_out_worker, &task_attributes,
                                &this->_usb_out_thread, this);
        if (res)
        {
            fprintf(stderr, "Error opening file: %s\n", strerror(res));
            return -1;
        }

        return 0;
    }

    int _engine_buffer_size_frames = 0;
    int _num_channels = 0;
    unsigned _sample_rate = 0;

    pthread_t _usb_out_worker;
    pthread_t _usb_in_worker;

    std::atomic_bool _is_usb_running = {false};

    std::unique_ptr<int32_t[]> _usb_audio_out_buff;
    std::unique_ptr<int32_t[]> _usb_audio_in_buff;

    int _raspa_out_buf_idx = 0; //increments every raspa buffer size
    int _raspa_in_buf_idx = 0; //increments every raspa buffer size

    snd_pcm_format_t _snd_format = ALSA_USB_SND_PCM_FORMAT;
    snd_pcm_uframes_t _alsa_buffer_size_frames = 0;
    snd_pcm_uframes_t _alsa_period_size_frames = 0;
    snd_pcm_hw_params_t *_snd_hw_params = nullptr;
    snd_pcm_sw_params_t *_snd_sw_params = nullptr;
    snd_output_t *_snd_output = NULL;
    snd_pcm_t *_pcm_playback_handle;
    snd_pcm_t *_pcm_capture_handle;
    CircularFifo<int32_t*, RASPA_TO_USB_IO_BUFFER_RATIO>_input_usb_fifo;
    CircularFifo<int32_t*, RASPA_TO_USB_IO_BUFFER_RATIO>_output_usb_fifo;
};

}

#endif // RASPA_ALSA_USB_H_
