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
    constexpr char RASPA_USB_ALSA_DEVICE[] = "sysdefault:CARD=UAC2Gadget";
    
    //Num of ALSA buffers per each raspa buffer.(ALSA is working in nrt domain and it ll be slower than raspa)
    constexpr int RASPA_TO_ALSA_PERIOD_RATIO = 2;
    
    // Num of periods per ALSA buffer (Num of times ALSA wakes up per buffer)
    constexpr int ALSA_PERIOD_TO_BUFFER_RATIO = 4;

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

        snd_output_stdio_attach(&_snd_output, stdout, 0);
        auto ret = snd_pcm_open(&_pcm_handle, RASPA_USB_ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
        if (ret)
        {
            printf("snd_pcm_open failed!\n");
            return ret;
        }
        ret = _set_hw_params(_pcm_handle, _snd_hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
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
        snd_pcm_dump(_pcm_handle, _snd_output);
        return 0;
    }

    int start_usb_streams()
    {
        _is_running = true;
        _alsa_worker_start();
        return 0;
    }

    int* get_usb_out_buffer_for_raspa()
    {
        return &_usb_audio_out_buff[_raspa_common_buf_idx * _engine_buffer_size_frames * _num_channels];
    }

    int* get_usb_in_buffer_for_raspa()
    {
        return &_usb_audio_in_buff[_raspa_common_buf_idx * _engine_buffer_size_frames * _num_channels];
    }

    void increment_buf_indices()
    {
        _alsa_common_period_idx = _raspa_common_buf_idx/RASPA_TO_ALSA_PERIOD_RATIO;
        _raspa_common_buf_idx++;
        _raspa_common_buf_idx %= (RASPA_TO_ALSA_PERIOD_RATIO *
                                    ALSA_PERIOD_TO_BUFFER_RATIO);
    }

    int close()
    {
        _is_running = false;
        snd_pcm_close(_pcm_handle);
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

    static int _xrun_recovery(snd_pcm_t *handle, int err)
    {
        if (err == -EPIPE)
        {    /* under-run */
            printf("under-run !\n");
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
            return 0;
        } else if (err == -ESTRPIPE)
        {
            while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            {
                sleep(1);
            }
            if (err < 0)
            {
                err = snd_pcm_prepare(handle);
                if (err < 0)
                    printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
            }
            return 0;
        }
        return err;
    }

    int _set_hw_params(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access)
    {
        unsigned int rrate;
        snd_pcm_uframes_t size;
        int err, dir;
        int resample = 0;

        err = snd_pcm_hw_params_any(handle, params);
        if (err < 0)
        {
            return err;
        }
        err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
        if (err < 0)
        {
            return err;
        }
        err = snd_pcm_hw_params_set_access(handle, params, access);
        if (err < 0)
        {
            return err;
        }
        err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S32_LE);
        if (err < 0)
        {
            return err;
        }
        err = snd_pcm_hw_params_set_channels(handle, params, _num_channels);
        if (err < 0)
        {
            return err;
        }
        rrate = _sample_rate;
        err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
        if (err < 0)
        {
            return err;
        }
        if (rrate != _sample_rate)
        {
            return -EINVAL;
        }
        err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &_alsa_buffer_size_frames);
        if (err < 0)
        {
            return err;
        }
        err = snd_pcm_hw_params_get_buffer_size(params, &size);
        if (err < 0)
        {
            return err;
        }
        _alsa_buffer_size_frames = size;

        err = snd_pcm_hw_params_set_period_size_near(handle, params, &_alsa_period_size_frames, &dir);
        if (err < 0)
        {
            return err;
        }
        err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
        if (err < 0)
        {
            return err;
        }
        _alsa_period_size_frames = size;
        err = snd_pcm_hw_params(handle, params);
        if (err < 0)
        {
            return err;
        }

        printf("period = %ld, buffer = %ld\n", _alsa_period_size_frames, _alsa_buffer_size_frames);
        return 0;
    }

    int _set_sw_params(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
    {
        int err;

        err = snd_pcm_sw_params_current(handle, swparams);
        if (err < 0)
        {
            return err;
        }

        err = snd_pcm_sw_params_set_start_threshold(handle, swparams, _alsa_period_size_frames * 2);
        if (err < 0)
        {
            return err;
        }

        err = snd_pcm_sw_params_set_avail_min(handle, swparams, _alsa_period_size_frames);
        if (err < 0)
        {
            return err;
        }

        err = snd_pcm_sw_params(handle, swparams);
        if (err < 0)
        {
            return err;
        }
        return 0;
    }

    void* _usb_output_worker()
    {
        signed int *frame_ptr;
        int err, cptr;

        while (_is_running)
        {
            frame_ptr = &_usb_audio_out_buff[_alsa_common_period_idx * _alsa_period_size_frames * _num_channels];
            cptr = _alsa_period_size_frames;
            while (cptr > 0)
            {
                err = snd_pcm_mmap_writei(_pcm_handle, frame_ptr, cptr);
                if (err == -EAGAIN)
                    continue;
                if (err < 0)
                {
                    _xrun_recovery(_pcm_handle, err);
                    break;
                }
                frame_ptr += err * _num_channels;
                cptr -= err;
            }
        }
        return nullptr;
    }

    void _usb_input_worker()
    {

    }

    static void* _usb_out_thread(void* data)
    {
        auto raspa_alsa_usb = reinterpret_cast<RaspaAlsaUsb *>(data);
        return raspa_alsa_usb->_usb_output_worker();
    }

    int _alsa_worker_start()
    {
        struct sched_param rt_params = {.sched_priority = 50};
        pthread_attr_t task_attributes;
        pthread_attr_init(&task_attributes);

        pthread_attr_setdetachstate(&task_attributes, PTHREAD_CREATE_JOINABLE);
        pthread_attr_setinheritsched(&task_attributes, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&task_attributes, SCHED_FIFO);
        pthread_attr_setschedparam(&task_attributes, &rt_params);

        int res = pthread_create(&_usb_out_worker, &task_attributes,
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

    std::atomic_bool _is_running = {false};

    std::unique_ptr<int32_t[]> _usb_audio_out_buff;
    std::unique_ptr<int32_t[]> _usb_audio_in_buff;

    int _raspa_common_buf_idx = 0; //increments every raspa buffer size
    int _alsa_common_period_idx = 0; //increments every alsa period

    snd_pcm_uframes_t _alsa_buffer_size_frames = 0;
    snd_pcm_uframes_t _alsa_period_size_frames = 0;
    snd_pcm_hw_params_t *_snd_hw_params = nullptr;
    snd_pcm_sw_params_t *_snd_sw_params = nullptr;
    snd_output_t *_snd_output = NULL;
    snd_pcm_sframes_t _frames_delay;
    snd_pcm_sframes_t _frames_available;
    snd_pcm_t *_pcm_handle;
};

}

#endif // RASPA_ALSA_USB_H_
