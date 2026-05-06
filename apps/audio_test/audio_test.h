/**
 * @brief Example of public header accessible from e.g. different build targets
 *
 */

#ifndef AUDIO_TEST_H
#define AUDIO_TEST_H

#include <vector>
#include <optional>
#include <span>
#include <atomic>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpsabi"
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#pragma GCC diagnostic pop

class BaseClassTestCase;

namespace at
{

struct TestConfig
{
    int channels;
    int length;
    int delay_samples;
    float sample_rate;
    float input_gain_db = 0.0f;
    float output_gain_db = 0.0f;
    float test_level_db = -3.0f;
    std::vector<int> channel_mapping;
    std::vector<int> ignored_channels;
};

class Report
{
public:
    virtual ~Report() = default;
    virtual bool passed();
    virtual json details();
protected:
    Report() = default;
    explicit Report(const std::vector<int>& ignored_channels);
    bool _is_ignored(int channel) const;
    std::vector<int> _ignored_channels;
};

class Test
{
public:
    Test(const TestConfig& config);
    virtual ~Test();

    virtual bool record_audio(const float* audio, int frames);
    virtual bool generate_audio_to(float* audio, int frames);
    virtual bool has_enough_samples();
    virtual std::unique_ptr<Report> analyse();

    void set_delay(int delay);
    int channels() const;

    std::chrono::milliseconds duration() const;

protected:
    int _recorded_samples{0};
    int _generated_samples{0};
    int _audio_length{0};
    int _delay_samples{0};
    float _sample_rate{0.0f};
    float _input_gain_db{0.0f};
    float _output_gain_db{0.0f};
    float _test_level_db{0.0f};
    std::atomic_bool _done{false};
    std::vector<std::vector<float>> _recorded_audio;
    std::vector<int> _channel_map;
    std::vector<int> _ignored_channels;

    friend class BaseClassTest;
};

/* Does RMS measurements of the noise floor and checks if it's below a given value */
class NoiseTest : public Test
{
public:
    static constexpr int DEFAULT_LENGTH = 65536;
    static constexpr int DEFAULT_THRESHOLD = -90;

    class NoiseReport : public Report
    {
        public:
        NoiseReport() = default;
        NoiseReport(const std::vector<int>& ignored_channels, std::vector<float>&& noise_db, int noise_level);
        bool passed() override;

        json details() override;
        float noise_level(int channel);

    private:
        float _noise_pass_level_db{0.0f};
        std::vector<float> _noise_db;
    };

    NoiseTest(const TestConfig& config, float noise_threshold_db);

    std::unique_ptr<Report> analyse() override;

private:
    float _noise_pass_level_db{0.0f};
};

/* Sine sweep for testing frequency response */
class FrequencyTest : public Test
{
public:
    static constexpr int DEFAULT_LENGTH = 262144; // 5.8 seconds @ 48kHz
    static constexpr int DEFAULT_THRESHOLD = 3;
    static constexpr float DEFAULT_LOW = 20;
    static constexpr float DEFAULT_HIGH = 20000;

    class FrequencyReport : public Report
    {
    public:
        FrequencyReport() = default;
        FrequencyReport(const std::vector<int>& ignored_channels, std::vector<std::vector<float>>&& ampl, int margin_db, float low_cutoff, float high_cutoff, bool print_fft);
        bool passed() override;

        json details() override;
        const std::vector<float>& frequency_response(int channel) const ;
        std::pair<float, float> min_max_values(int channel) const;

    private:
        std::vector<std::vector<float>> _responses;
        float _margin_db{0.0f};
        float _low_cutoff{0.0f};
        float _high_cutoff{0.0f};
        bool  _print_fft{false};
    };

    FrequencyTest(const TestConfig& config, float start_freq, float end_freq, float margin_db, bool print_fft);

    float _generate_sine_sweep();

    bool generate_audio_to(float* audio, int frames) override;

    std::unique_ptr<Report> analyse() override;

private:
    static constexpr int SWEEP_FADE_OUT = 100;

    float _start_freq{0.0f};
    float _end_freq{0.0f};
    float _margin_db{0.0f};
    float _noise_pass_level_db{0.0f};
    float _phase{0.0f};
    float _amplitude{1.0f};
    bool _print_fft{false};
};

/* Plays a Hann-windowed sine on one channel at a time; verifies channel balance and crosstalk */
class LevelTest : public Test
{
public:
    static constexpr int DEFAULT_LENGTH = 65536;
    static constexpr int DEFAULT_LEVEL_MARGIN = 1;
    static constexpr int DEFAULT_CROSSTALK = -60;

    static constexpr float TEST_TONE_FREQUENCY = 1000.0f;

    class LevelReport : public Report
    {
    public:
        LevelReport() = default;
        explicit LevelReport(const std::vector<int>& ignored_channels) : Report(ignored_channels) {}
        LevelReport(const std::vector<int>& ignored_channels,
                    std::vector<std::vector<float>>&& rms_db,
                    std::vector<std::vector<float>>&& peak_db,
                    float margin_db,
                    float crosstalk_threshold_db,
                    float reference_level_db);
        bool passed() override;

        json details() override;
        float level_db(int channel) const;
        float crosstalk_db(int from_channel, int to_channel) const;

    private:
        std::vector<std::vector<float>> _rms_db;
        std::vector<std::vector<float>> _peak_db;
        float _margin_db{0.0f};
        float _crosstalk_threshold_db{0.0f};
        float _reference_level_db{0.0f};
    };

    LevelTest(const TestConfig& config, float level_margin_db, float crosstalk_threshold_db);

    bool generate_audio_to(float* audio, int frames) override;

    std::unique_ptr<Report> analyse() override;

protected:
    float _amplitude{1.0f};
    float _margin_db{0.0f};
    float _crosstalk_threshold_db{0.0f};
};

class MuteTest : public LevelTest
{
public:
    static constexpr float DEFAULT_THRESHOLD = -90.0f;
    static constexpr int DEFAULT_LENGTH = 65536;

    class MuteReport : public LevelReport
    {
    public:
        MuteReport() = default;
        MuteReport(const std::vector<int>& ignored_channels,
                   std::vector<float>&& peak_db,
                   float margin_db,
                   float reference_level_db);
        bool passed() override;
        float level_db(int channel) const;
        json details() override;

    private:
        std::vector<float> _peak_db;
        float _margin_db{0.0f};
        float _reference_level_db{0.0f};
    };

    MuteTest(const TestConfig& config, float level_margin_db);

    std::unique_ptr<Report> analyse() override;
};

/* Generates a Hann-windowed sine on all channels simultaneously and checks that
 * all spectral components outside the fundamental are below a given threshold */
class DistortionTest : public Test
{
public:
    static constexpr int   DEFAULT_LENGTH    = 65536;
    static constexpr float DEFAULT_THRESHOLD = -60.0f;
    static constexpr float TEST_TONE_FREQUENCY = 750;

    struct ChannelResult
    {
        float peak_db;
        float max_distortion_db;
        float ratio_db;
        bool  passed;
    };

    class DistortionReport : public Report
    {
    public:
        DistortionReport() = default;
        DistortionReport(const std::vector<int>& ignored_channels,
                         std::vector<float>&& ratios,
                         std::vector<std::vector<float>>&& graphs,
                         float threshold_db,
                         bool print_fft);
        bool passed() override;
        json details() override;

    private:
        std::vector<float> _ratios;
        std::vector<std::vector<float>> _graphs;
        float _threshold_db{0.0f};
        bool _print_fft{false};
    };

    DistortionTest(const TestConfig& config, float threshold_db, bool print_fft);

    bool generate_audio_to(float* audio, int frames) override;

    std::unique_ptr<Report> analyse() override;

private:
    static constexpr int EXCLUSION_BINS = 50;

    float _threshold_db;
    bool  _print_fft;
    float _amplitude{1.0f};
};

/* Measures round-trip delay by cross-correlating a generated impulse with the recorded signal */
class DelayTest : public Test
{
public:
    static constexpr int DEFAULT_LENGTH = 4096;
    class DelayReport : public Report
    {
    public:
        DelayReport() = default;
        DelayReport(const std::vector<int>& ignored_channels, std::vector<int>&& delays_samples, int expected_delay, int tolerance);
        bool passed() override;

        json details() override;
        int delay_samples(int channel) const;

    private:
        std::vector<int> _delays_samples;
        int _expected_delay{0};
        int _tolerance{0};
    };

    DelayTest(const TestConfig& config, int expected_delay_samples, int tolerance_samples, int blocksize);

    bool generate_audio_to(float* audio, int frames) override;

    std::unique_ptr<Report> analyse() override;

private:
    static constexpr int PULSE_WIDTH = 10;

    int _expected_delay_samples{0};
    int _tolerance_samples{0};
    int _blocksize{0};
    float _amplitude{1.0f};
};

}


#endif //AUDIO_TEST_H
