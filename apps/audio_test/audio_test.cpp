#include <algorithm>
#include <bit>
#include <cassert>
#include <iostream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "audio_test.h"
#include "analysis.h"

namespace at {

constexpr int FFT_REPORT_SIZE = 1024;

Report::Report(const std::vector<int>& ignored_channels) : _ignored_channels(ignored_channels)
{}

bool Report::_is_ignored(int channel) const
{
    return std::find(_ignored_channels.begin(), _ignored_channels.end(), channel) != _ignored_channels.end();
}

bool Report::passed()
{
    return false;
}

json Report::details()
{
    return json();
}

Test::Test(const TestConfig& config)
{
    _delay_samples = config.delay_samples;
    _audio_length = static_cast<int>(std::bit_ceil(static_cast<unsigned>(config.length))); // Round up to a power of 2
    _recorded_audio= std::vector<std::vector<float>>(config.channels);
    _sample_rate = config.sample_rate;
    _input_gain_db = config.input_gain_db;
    _output_gain_db = config.output_gain_db;
    _test_level_db = config.test_level_db;
    _channel_map = config.channel_mapping;
    _ignored_channels = config.ignored_channels;
    for (auto& c: _recorded_audio)
    {
        c.reserve(_audio_length);
    }
}

Test::~Test()
{
}

bool Test::record_audio(const float* audio, int frames)
{
    int channels = _recorded_audio.size();
    int start = std::min(frames, _delay_samples);
    int end = frames - std::max(0, _recorded_samples + frames - _audio_length);

    int frames_to_copy = end - start;
    assert(frames_to_copy <= frames);
    assert(frames_to_copy >= 0);

    for (int channel = 0; channel < channels; ++channel)
    {
        const float* ch_data = audio + channel * frames;
        auto& target = _recorded_audio[_channel_map[channel]];
        target.insert(target.end(), ch_data + start, ch_data + end);
    }
    _recorded_samples += frames_to_copy;
    _delay_samples = std::max(0, _delay_samples - (frames - frames_to_copy));

    assert(_recorded_samples <= _audio_length);
    assert(_recorded_audio[0].size() <= _audio_length);
    if (_recorded_samples == _audio_length)
    {
        _done = true;
    }
    return _done;
}

bool Test::generate_audio_to(float* audio, int frames)
{
    std::fill(audio, audio + _recorded_audio.size() * frames, 0.0f);
    _generated_samples += frames;
    return false;
}

bool Test::has_enough_samples()
{
    return _done;
}

std::unique_ptr<Report> Test::analyse()
{
    return nullptr;
}

void Test::set_delay(int delay)
{
    _delay_samples = delay;
}

int Test::channels() const
{
    return _recorded_audio.size();
}

std::chrono::milliseconds Test::duration() const
{
    return std::chrono::milliseconds(static_cast<int>(1000.0 * _audio_length / _sample_rate));
}

NoiseTest::NoiseReport::NoiseReport(const std::vector<int>& ignored_channels, std::vector<float>&& noise_db, int noise_level) :
        Report(ignored_channels), _noise_pass_level_db(noise_level), _noise_db(std::move(noise_db))
{}

bool NoiseTest::NoiseReport::passed()
{
    for (int channel = 0; channel < static_cast<int>(_noise_db.size()); ++channel)
    {
        if (_noise_db[channel] >= _noise_pass_level_db && !_is_ignored(channel))
        {
            return false;
        }
    }
    return true;
}

json NoiseTest::NoiseReport::details()
{
    json details;
    details["test_name"] = "Background noise test";
    details["passed"] = passed();
    details["ignored_channels"] = _ignored_channels;
    details["levels_db"] = _noise_db;
    details["noise_threshold_db"] = _noise_pass_level_db;
    return details;
}

float NoiseTest::NoiseReport::noise_level(int channel)
{
    if (channel < _noise_db.size() && channel >= 0 )
    {
        return _noise_db.at(channel);
    }
    return 0;
}

NoiseTest::NoiseTest(const TestConfig& config, float noise_pass_level_db) : Test(config),
                                                                            _noise_pass_level_db(noise_pass_level_db)
{
}

std::unique_ptr<Report> NoiseTest::analyse()
{
    std::vector<float> noise_db;
    for (int i= 0; i < _recorded_audio.size(); ++i)
    {
        noise_db.push_back(to_db(calc_rms(_recorded_audio[i])));
    }

    auto report = std::make_unique<NoiseReport>(_ignored_channels, std::move(noise_db), _noise_pass_level_db);
    return report;
}


FrequencyTest::FrequencyReport::FrequencyReport(const std::vector<int>& ignored_channels,
    std::vector<std::vector<float>>&& ampl, int margin_db, float low_cutoff, float high_cutoff, bool print_fft) :
        Report(ignored_channels), _responses(std::move(ampl)), _margin_db(margin_db), _low_cutoff(low_cutoff), _high_cutoff(high_cutoff), _print_fft(print_fft)
{}

const std::vector<float>& FrequencyTest::FrequencyReport::frequency_response(int channel) const
{
    assert(channel < _responses.size() && channel >= 0);
    return _responses[channel];
}

std::pair<float, float> FrequencyTest::FrequencyReport::min_max_values(int channel) const
{
    assert(channel < _responses.size() && channel >= 0);
    auto [min, max] = std::minmax_element(_responses[channel].begin() + _low_cutoff, _responses[channel].begin() + _high_cutoff);
    return {*min, *max};
}

bool FrequencyTest::FrequencyReport::passed()
{
    float margin = db_to_gain(std::abs(_margin_db) * 2);
    for (int channel = 0; channel < static_cast<int>(_responses.size()); ++channel)
    {
        auto [min, max] = this->min_max_values(channel);
        if (max / min > margin && !_is_ignored(channel))
        {
            return false;
        }
    }
    return true;
}

json FrequencyTest::FrequencyReport::details()
{
    json details;
    details["test_name"] = "Frequency response test";
    details["passed"] = passed();
    details["ignored_channels"] = _ignored_channels;
    details["margin_db"] = _margin_db;
    json channels = json::array();
    for (int i = 0; i < static_cast<int>(_responses.size()); ++i)
    {
        auto [min, max] = min_max_values(i);
        channels.push_back({{"min_db", to_db(min)}, {"max_db", to_db(max)}});
    }
    if (_print_fft)
    {
        details["fft"] = _responses;
    }
    details["response"] = channels;
    return details;
}

FrequencyTest::FrequencyTest(const TestConfig& config, float start_freq, float end_freq, float margin_db, bool print_fft) :
        Test(config), _start_freq{start_freq}, _end_freq{end_freq}, _margin_db(margin_db), _print_fft(print_fft)
{
    _amplitude = db_to_gain(_test_level_db);
}

float FrequencyTest::_generate_sine_sweep()
{
    float freq = static_cast<float>(_generated_samples) / (_audio_length - SWEEP_FADE_OUT) * ( _end_freq - _start_freq) + _start_freq;
    float phase_inc =  freq / _sample_rate;
    float fade_out_window = (_generated_samples >= _audio_length - SWEEP_FADE_OUT)? hann_window(_audio_length - _generated_samples, SWEEP_FADE_OUT * 2) : 1.0f;
    _phase += phase_inc;
    if (_phase >= 1.0f)
    {
        _phase -= 1.0f;
    }
    _generated_samples++;
    return std::sin(_phase * 2.0f * M_PI) * _amplitude * fade_out_window;
}

bool FrequencyTest::generate_audio_to(float* audio, int frames)
{
    int channels = _recorded_audio.size();
    for (int i = 0; i < frames; ++i)
    {
        float sample = _generate_sine_sweep();
        for (int ch = 0; ch < channels; ++ch)
        {
            audio[ch * frames + i] = sample;
        }
    }
    return _generated_samples >= _audio_length;
}

static std::vector<float> reduce_vector(const std::vector<float>& input, int target_length)
{
    if (static_cast<int>(input.size()) <= target_length)
    {
        return input;
    }

    std::vector<float> output(target_length);
    float step = static_cast<float>(input.size() - 1) / static_cast<float>(target_length - 1);
    for (int i = 0; i < target_length; ++i)
    {
        float pos = i * step;
        int lo = static_cast<int>(pos);
        int hi = lo + 1 < static_cast<int>(input.size()) ? lo + 1 : lo;
        float t = pos - static_cast<float>(lo);
        output[i] = input[lo] * (1.0f - t) + input[hi] * t;
    }
    return output;
}

std::unique_ptr<Report> FrequencyTest::analyse()
{
    std::vector<std::vector<float>> freq_resp;

    for (int i= 0; i < _recorded_audio.size(); ++i)
    {
        auto bins = complex_abs(fft(_recorded_audio[i]));
        // fft is symmetric so we only need half of it.
        auto half = std::vector<float>(bins.begin(), bins.begin() + bins.size() / 2);
        normalize(half);
        freq_resp.push_back(reduce_vector(half, FFT_REPORT_SIZE));
    }
    /* The cutoff points are nudged ever so slightly towards the "middle" rather than the exact
     * start and stop frequencies of the test. This is to avoid artefacts at the edges */
    int low_cutoff = std::max(std::ceil(2 * (_start_freq / _sample_rate) * freq_resp.at(0).size()), 2.0f);
    int high_cutoff = std::floor(1.98 * (_end_freq / _sample_rate) * freq_resp.at(0).size());

    auto report = std::make_unique<FrequencyReport>(_ignored_channels, std::move(freq_resp), _margin_db, low_cutoff, high_cutoff, _print_fft);
    return report;
}


LevelTest::LevelReport::LevelReport(const std::vector<int>& ignored_channels,
                                    std::vector<std::vector<float>>&& rms_db,
                                    std::vector<std::vector<float>>&& peak_db,
                                    float margin_db,
                                    float crosstalk_threshold_db,
                                    float reference_level_db) :
        Report(ignored_channels), _rms_db(std::move(rms_db)), _peak_db(std::move(peak_db)), _margin_db(margin_db),
        _crosstalk_threshold_db(crosstalk_threshold_db), _reference_level_db(reference_level_db)
{}

bool LevelTest::LevelReport::passed()
{
    bool level_ok = true;
    for (int channel = 0; channel < static_cast<int>(_peak_db.size()); ++channel)
    {
        if (std::abs(_peak_db[channel][channel] - _reference_level_db) > _margin_db && !_is_ignored(channel))
        {
            level_ok = false;
        }
    }

    bool crosstalk_ok = true;
    for (int channel = 0; channel < static_cast<int>(_rms_db.size()); ++channel)
    {
        for (int other = 0; other < static_cast<int>(_rms_db.size()); ++other)
        {
            if (channel != other && !_is_ignored(other) && !_is_ignored(channel))
            {
                if (_rms_db[channel][other] - _rms_db[channel][channel] > _crosstalk_threshold_db)
                {
                    crosstalk_ok = false;
                }
            }
        }
    }
    return level_ok && crosstalk_ok;
}

json LevelTest::LevelReport::details()
{
    json details;
    details["test_name"] = "Level and crosstalk test";
    details["passed"] = passed();
    details["ignored_channels"] = _ignored_channels;
    details["level_threshold_db"] = _margin_db;
    details["crosstalk_threshold_db"] = _crosstalk_threshold_db;
    details["levels_rms"] = _rms_db;
    details["levels_peak"] = _peak_db;
    return details;
}

float LevelTest::LevelReport::level_db(int channel) const
{
    assert(channel >= 0 && channel < static_cast<int>(_peak_db.size()));
    return _peak_db[channel][channel];
}

float LevelTest::LevelReport::crosstalk_db(int from_channel, int to_channel) const
{
    assert(from_channel >= 0 && from_channel < static_cast<int>(_rms_db.size()));
    assert(to_channel >= 0 && to_channel < static_cast<int>(_rms_db[from_channel].size()));
    return _rms_db[from_channel][to_channel];
}

LevelTest::LevelTest(const TestConfig& config, float level_margin_db,  float crosstalk_threshold_db) :
        Test(config),
        _margin_db(level_margin_db),
        _crosstalk_threshold_db(crosstalk_threshold_db)
{
    _amplitude = db_to_gain(_test_level_db);
}

bool LevelTest::generate_audio_to(float* audio, int frames)
{
    int channels = _recorded_audio.size();
    int segment_length = _audio_length / channels;
    for (int i = 0; i < frames; ++i)
    {
        int active_ch = _generated_samples / segment_length;
        int pos_in_seg = _generated_samples % segment_length;
        float sample = std::sin(2.0f * M_PI * TEST_TONE_FREQUENCY * pos_in_seg / _sample_rate) * hann_window(pos_in_seg, segment_length) * _amplitude;
        for (int ch = 0; ch < channels; ++ch)
        {
            audio[ch * frames + i] = (ch == active_ch) ? sample : 0.0f;
        }
        _generated_samples++;
    }
    return _generated_samples >= _audio_length;
}

std::unique_ptr<Report> LevelTest::analyse()
{
    int channels = _recorded_audio.size();
    int segment_length = _audio_length / channels;
    std::vector<float> levels_db(channels);
    std::vector<std::vector<float>> rms_db(channels, std::vector<float>(channels));
    std::vector<std::vector<float>> peak_db(channels, std::vector<float>(channels));

    for (int active_ch = 0; active_ch < channels; ++active_ch)
    {
        int seg_start = active_ch * segment_length;

        for (int ch = 0; ch < channels; ++ch)
        {
            auto seg_begin = _recorded_audio[ch].begin() + seg_start;
            auto seg_end = seg_begin + segment_length;

            auto [mn, mx] = std::minmax_element(seg_begin, seg_end);
            float peak = std::max(std::abs(*mn), std::abs(*mx));
            peak_db[active_ch][ch] = to_db(peak);

            std::vector<float> segment(seg_begin, seg_end);
            rms_db[active_ch][ch] = to_db(calc_rms(segment));
        }
    }
    auto report = std::make_unique<LevelReport>(_ignored_channels, std::move(rms_db), std::move(peak_db), _margin_db, _crosstalk_threshold_db, _test_level_db);
    return report;
}


MuteTest::MuteReport::MuteReport(const std::vector<int>& ignored_channels, std::vector<float>&& peak_db, float margin_db, float reference_level_db) :
     LevelReport(ignored_channels), _peak_db(std::move(peak_db)), _margin_db(margin_db), _reference_level_db(reference_level_db)
{}

bool MuteTest::MuteReport::passed()
{
    for (int channel = 0; channel < static_cast<int>(_peak_db.size()); ++channel)
    {
        if (_peak_db[channel] > _margin_db && !_is_ignored(channel))
        {
            return false;
        }
    }
    return true;
}

float MuteTest::MuteReport::level_db(int channel) const
{
    assert(channel >= 0 && channel < _peak_db.size());
    return _peak_db[channel];
}

json MuteTest::MuteReport::details()
{
    json details;
    details["test_name"] = "Mute test";
    details["passed"] = passed();
    details["ignored_channels"] = _ignored_channels;
    details["level_threshold_db"] = _margin_db;
    details["levels_peak_db"] = _peak_db;
    return details;
}

MuteTest::MuteTest(const TestConfig& config, float level_margin_db) : LevelTest(config, level_margin_db, 0.0f)
{
}

std::unique_ptr<Report> MuteTest::analyse()
{
    int channels = _recorded_audio.size();
    int segment_length = _audio_length / channels;
    std::vector<float> peak_db;

    for (int channel = 0; channel < channels; ++channel)
    {
        auto seg_begin = _recorded_audio[channel].begin() + channel * segment_length;
        auto seg_end = seg_begin + segment_length;
        auto [mn, mx] = std::minmax_element(seg_begin, seg_end);
        float peak = std::max(std::abs(*mn), std::abs(*mx));
        peak_db.push_back(to_db(peak));
    }
    auto report = std::make_unique<MuteReport>(_ignored_channels, std::move(peak_db), _margin_db, _test_level_db);
    return report;
}

DistortionTest::DistortionReport::DistortionReport(const std::vector<int>& ignored_channels,
                                                   std::vector<float>&& ratios,
                                                   std::vector<std::vector<float>>&& graphs,
                                                   float threshold_db,
                                                   bool print_fft) :
    Report(ignored_channels), _ratios(std::move(ratios)), _graphs(std::move(graphs)), _threshold_db(threshold_db), _print_fft(print_fft)
{}

bool DistortionTest::DistortionReport::passed()
{
    for (int channel = 0; channel < static_cast<int>(_ratios.size()); ++channel)
    {
        if (_ratios[channel] < std::abs(_threshold_db) && !_is_ignored(channel))
        {
            return false;
        }
    }
    return true;
}

json DistortionTest::DistortionReport::details()
{
    json details;
    details["test_name"] = "Distortion test";
    details["passed"] = passed();
    details["ignored_channels"] = _ignored_channels;
    details["threshold_db"] = _threshold_db;
    details["ratios"] = _ratios;
    if (_print_fft)
    {
        details["fft"] = _graphs;
    }
    return details;
}

DistortionTest::DistortionTest(const TestConfig& config, float threshold_db, bool print_fft) :
    Test(config), _threshold_db(threshold_db), _print_fft(print_fft)
{
    _amplitude = db_to_gain(_test_level_db);
}

bool DistortionTest::generate_audio_to(float* audio, int frames)
{
    int channels = _recorded_audio.size();
    for (int i = 0; i < frames; ++i)
    {
        float sample = std::sin(2.0f * M_PI * TEST_TONE_FREQUENCY * _generated_samples / _sample_rate)
                     * hann_window(_generated_samples, _audio_length)
                     * _amplitude;
        for (int ch = 0; ch < channels; ++ch)
        {
            audio[ch * frames + i] = sample;
        }
        _generated_samples++;
    }
    return _generated_samples >= _audio_length;
}

std::unique_ptr<Report> DistortionTest::analyse()
{
    std::vector<float> levels;
    std::vector<std::vector<float>> graphs;

    for (int i = 0; i < static_cast<int>(_recorded_audio.size()); ++i)
    {
        auto bins = complex_abs(fft(_recorded_audio[i]));
        // FFT is symmetric, take first half
        auto half = std::vector<float>(bins.begin(), bins.begin() + bins.size() / 2);
        normalize(half);
        half[0] = 0.0f; // discard DC

        // Find the peak bin (expected near 1kHz fundamental)
        int peak_bin = static_cast<int>(std::distance(half.begin(), std::max_element(half.begin(), half.end())));
        float peak_mag = half[peak_bin];

        float max_other_mag = 0.0f;
        for (int b = 0; b < static_cast<int>(half.size()); ++b)
        {
            if (std::abs(b - peak_bin) > EXCLUSION_BINS)
            {
                max_other_mag = std::max(max_other_mag, half[b]);
            }
        }
        levels.push_back(to_db(peak_mag) - to_db(max_other_mag));
        graphs.push_back(reduce_vector(half, FFT_REPORT_SIZE));
    }

    auto report = std::make_unique<DistortionReport>(_ignored_channels, std::move(levels), std::move(graphs), _threshold_db, _print_fft);
    return report;
}


DelayTest::DelayReport::DelayReport(const std::vector<int>& ignored_channels, std::vector<int>&& delays_samples, int expected_delay, int tolerance) :
        Report(ignored_channels), _delays_samples(std::move(delays_samples)), _expected_delay(expected_delay), _tolerance(tolerance)
{}

bool DelayTest::DelayReport::passed()
{
    for (int channel = 0; channel < static_cast<int>(_delays_samples.size()); ++channel)
    {
        if (std::abs(_delays_samples[channel] - _expected_delay) > _tolerance && !_is_ignored(channel))
        {
            return false;
        }
    }
    return true;
}

json DelayTest::DelayReport::details()
{
    json details;
    details["test_name"] = "Round-trip delay test";
    details["passed"] = passed();
    details["ignored_channels"] = _ignored_channels;
    details["expected_delay_samples"] = _expected_delay;
    details["tolerance_samples"] = _tolerance;
    details["measured_delay_samples"] = _delays_samples;
    return details;
}

int DelayTest::DelayReport::delay_samples(int channel) const
{
    assert(channel >= 0 && channel < static_cast<int>(_delays_samples.size()));
    return _delays_samples[channel];
}

DelayTest::DelayTest(const TestConfig& config, int expected_delay_samples, int tolerance_samples, int blocksize) :
        Test(config), _expected_delay_samples(expected_delay_samples), _tolerance_samples(tolerance_samples), _blocksize(blocksize)
{
    _amplitude = db_to_gain(_test_level_db);
}

bool DelayTest::generate_audio_to(float* audio, int frames)
{
    int channels = _recorded_audio.size();
    for (int i = 0; i < frames; ++i)
    {
        float sample = (_generated_samples < PULSE_WIDTH) ? _amplitude : 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            audio[ch * frames + i] = sample;
        }
        _generated_samples++;
    }
    return _generated_samples >= _audio_length;
}

std::unique_ptr<Report> DelayTest::analyse()
{
    std::vector<int> delays;
    std::vector<float> reference_signal(_audio_length, 0.0f);
    std::fill(reference_signal.begin(), reference_signal.begin() + PULSE_WIDTH, 1.0f);

    std::vector<std::vector<float>> xcorrs;

    for (int i = 0; i < static_cast<int>(_recorded_audio.size()); ++i)
    {
        auto corr = xcorr(reference_signal, _recorded_audio[i]);
        normalize(corr);
        int peak_index = static_cast<int>(std::distance(corr.begin(), std::max_element(corr.begin(), corr.end())));
        int delay = peak_index + _blocksize - (static_cast<int>(reference_signal.size()) - 1);
        delays.push_back(delay);
    }
    auto report = std::make_unique<DelayReport>(_ignored_channels, std::move(delays), _expected_delay_samples, _tolerance_samples);
    return report;
}


}
