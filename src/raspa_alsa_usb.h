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

#include <pthread.h>
#include <alsa/asoundlib.h>

namespace {
    // Device name of the USB audio gadget listed by ALSA
    constexpr char RASPA_USB_ALSA_DEVICE[] = "default:CARD=UAC2Gadget";
    
    //Num of ALSA buffers per each raspa buffer.(ALSA is working in nrt domain and it ll be slower than raspa)
    constexpr unsigned RASPA_TO_ALSA_PERIOD_RATIO = 2;
    
    // Num of periods per ALSA buffer (Num of times ALSA wakes up per buffer)
    constexpr unsigned ALSA_PERIOD_TO_BUFFER_RATIO = 2;

    constexpr snd_pcm_format_t RASPA_PCM_FORMAT = SND_PCM_FORMAT_S32_LE;

}

namespace raspa {


class RaspaAlsaUsb
{
public:
    RaspaAlsaUsb() = default;
    ~RaspaAlsaUsb() = default;

   int init(int sample_rate, int engine_buffer_size, int num_channels)
   {
        _engine_buffer_size_frames = engine_buffer_size;
        _num_channels = num_channels;
        _sample_rate = sample_rate;
        _alsa_period_size_frames = _engine_buffer_size_frames * RASPA_TO_ALSA_PERIOD_RATIO;
        _alsa_buffer_size_frames = _alsa_period_size_frames * ALSA_PERIOD_TO_BUFFER_RATIO;

        _usb_audio_in_buff = std::make_unique<int32_t[]>(_alsa_buffer_size_frames * _num_channels);
        _usb_audio_out_buff = std::make_unique<int32_t[]>(_alsa_buffer_size_frames * _num_channels);

        snd_pcm_hw_params_alloca(&_snd_hw_params);
        snd_pcm_sw_params_alloca(&_snd_sw_params);

        snd_output_stdio_attach(&_snd_output_log, stdout, 0);
        auto ret = snd_pcm_open(&_pcm_handle, RASPA_USB_ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
        if (ret)
        {
            printf("snd_pcm_open failed!\n");
            return ret;
        }
        ret = _set_hw_params(_pcm_handle, _snd_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        if (ret)
        {
            printf("_set_hw_params failed!\n");
            return ret;
        }
        ret = _set_sw_params(_pcm_handle, _snd_sw_params);
        if (ret)
        {
            printf("_set_sw_params failed!\n");
            return ret;
        }
        printf("alsa usb init success.\n");
        return 0;
    }

    int start_usb_streams()
    {
        _is_running = true;
        _usb_out_thread = std::thread(&RaspaAlsaUsb::_usb_output_worker, this);
        return 0;
    }

    inline void set_usbout_index(int usb_out_idx)
    {
        _raspa_usbout_buf_idx = usb_out_idx;
    }

    inline void set_usbin_index(int usb_in_idx)
    {
        _raspa_usbin_buf_idx = usb_in_idx;
    }

    int close()
    {
        _is_running = false;
        if(snd_pcm_close(_pcm_handle))
        {
            printf("snd_pcm_close failed\n");
            return -1;
        }
        return 0;
    }

private:

    inline int _set_thread_fifo_priority(int priority)
    {
        struct sched_param params;
        params.sched_priority = priority;
        return pthread_setschedparam(pthread_self(), SCHED_FIFO, &params);
        return 0;
    }

    /* int _xrun_recovery(snd_pcm_t *handle, int err)
    {
        if (verbose)
            printf("stream recovery\n");
        if (err == -EPIPE)
        {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
            return 0;
        } else if (err == -ESTRPIPE)
        {
            while ((err = snd_pcm_resume(handle)) == -EAGAIN)
                sleep(1);
            if (err < 0)
            {
                err = snd_pcm_prepare(handle);
                if (err < 0)
                    printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
            }
            return 0;
        }
        return err;
    } */

    int _set_hw_params(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access)
    {
        snd_pcm_uframes_t size;
        int dir, resample = 1;
        snd_pcm_hw_params_any(handle, params);
        snd_pcm_hw_params_set_rate_resample(handle, params, resample);
        snd_pcm_hw_params_set_access(handle, params, access);
        snd_pcm_hw_params_set_format(handle, params, RASPA_PCM_FORMAT);
        snd_pcm_hw_params_set_channels(handle, params, _num_channels);                                        
        snd_pcm_hw_params_set_rate_near(handle, params, &_sample_rate, 0);
        snd_pcm_hw_params_set_buffer_size_near(handle, params, &_alsa_buffer_size_frames);
        snd_pcm_hw_params_get_buffer_size(params, &size);
        _alsa_buffer_size_frames = size;
        snd_pcm_hw_params_set_period_size_near(handle, params, &_alsa_period_size_frames, &dir);
        snd_pcm_hw_params_get_period_size(params, &size, &dir);
        _alsa_period_size_frames = size;
        auto ret = snd_pcm_hw_params(handle, params);
        printf("num_channels = %d buffer size = %ld, period = %ld\n", _num_channels,
                                            _alsa_buffer_size_frames, _alsa_period_size_frames);
        return ret;
    }

    int _set_sw_params(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
    {
        snd_pcm_sw_params_current(handle, swparams);
        snd_pcm_sw_params_set_start_threshold(handle, swparams, _alsa_period_size_frames);
        /* allow the transfer when at least period_size samples can be processed */
        snd_pcm_sw_params_set_avail_min(handle, swparams, _alsa_period_size_frames);
        return snd_pcm_sw_params(handle, swparams);
    }

    void _usb_output_worker()
    {
        signed int *frame_ptr;
        int ret, cptr;
        _set_thread_fifo_priority(50);
        while (_is_running)
        {
            frame_ptr = &_usb_audio_out_buff[_raspa_usbout_buf_idx * _alsa_period_size_frames
                                                                                 * _num_channels];
            cptr = _alsa_period_size_frames;
            while (cptr > 0)
            {

                ret = snd_pcm_writei(_pcm_handle, frame_ptr, cptr);
                snd_pcm_avail_delay(_pcm_handle,&_frames_available,&_frames_delay);
                if (ret == -EAGAIN)
                {
                    continue;
                } 
                frame_ptr += ret * _num_channels;
                cptr -= ret;
            }
        }
    }

    void _usb_input_worker()
    {

    }

    int _engine_buffer_size_frames = 0;
    int _num_channels = 0;
    unsigned _sample_rate = 0;
    
    std::thread _usb_out_thread;
    std::thread _usb_in_thread;

    bool _is_running;

    std::unique_ptr<int32_t[]> _usb_audio_out_buff;
    std::unique_ptr<int32_t[]> _usb_audio_in_buff;
    int _raspa_usbin_buf_idx = 0;
    int _raspa_usbout_buf_idx = 0;

    snd_pcm_uframes_t _alsa_buffer_size_frames = 0;
    snd_pcm_uframes_t _alsa_period_size_frames = 0;
    snd_pcm_hw_params_t *_snd_hw_params = nullptr;
    snd_pcm_sw_params_t *_snd_sw_params = nullptr;
    snd_output_t *_snd_output_log = NULL;
    snd_pcm_sframes_t _frames_delay;
    snd_pcm_sframes_t _frames_available;
    snd_pcm_t *_pcm_handle;
};

}

#endif // RASPA_ALSA_USB_H_
