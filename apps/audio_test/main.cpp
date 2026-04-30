#include <vector>
#include <fstream>
#include <iostream>
#include <cassert>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "options.h"
#ifdef DUMMY_AUDIO
typedef void (*RaspaProcessCallback)(float* input, float* output, void* data);
#else
#include <raspa/raspa.h>
#endif
#include "audio_test.h"

constexpr auto RETRY_TIME = std::chrono::milliseconds(100);
constexpr auto TEST_WAIT_TIME = std::chrono::milliseconds(500);
constexpr auto TEST_START_WAIT = std::chrono::milliseconds(3000);
constexpr int DUMMY_BUFFER_SIZE = 128;
constexpr float DUMMY_SAMPLERATE = 48000;
constexpr int DUMMY_CHANNELS = 2;
std::thread dummy_thread;
std::atomic_bool dummy_tread_flag=true;

struct Setup
{
    int channels = 2;
    int delay = 64;
    int buffersize = 64;
    bool autodetect_delay = false;
    bool print_fft = false;
    float input_gain = 0;
    float output_gain = 0;
    float test_level = -3;
    float noise_margin = -90;
    float crosstalk_margin = -80;
    float level_margin = 1;
    float mute_level = -60;
    std::string report_file = "./report.json";
    std::vector<std::string> tests;
    std::vector<std::pair<int, int>> channel_mappings;
    std::vector<int> ignored_channels;
    std::chrono::milliseconds initial_delay = TEST_START_WAIT;
};

struct RealtimeData
{
    int buffersize;
    int channels;
    at::Test* current_test;
    std::atomic_bool done;
};

void audio_callback(float* input, float* output, void* data)
{
    RealtimeData* rt_data = reinterpret_cast<RealtimeData*>(data);
    if (!rt_data->done)
    {
        at::Test* test = rt_data->current_test;
        int buffersize = rt_data->buffersize;

        test->generate_audio_to(output, buffersize);
        test->record_audio(input, buffersize);

        if (test->has_enough_samples())
        {
            rt_data->done = true;
        }
    }
    else
    {
        std::fill(output, output + rt_data->buffersize * rt_data->channels, 0.0f);
    }
}


void dummy_worker(RealtimeData* data, RaspaProcessCallback callback, int buffersize)
{
    std::vector<float> input(buffersize * DUMMY_CHANNELS, 0.0f);
    std::vector<float> output(buffersize * DUMMY_CHANNELS, 0.0f);
    while (dummy_tread_flag)
    {
        callback(input.data(), output.data(), reinterpret_cast<void*>(data));
        std::copy(output.begin(), output.end(), input.begin());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int init_dummy_audio(RealtimeData* data, int buffersize)
{
    dummy_thread = std::thread(dummy_worker, data, audio_callback, buffersize);
    return 0;
}

bool run_test(at::Test* test, RealtimeData* data)
{
    assert(data->done);
    data->current_test = test;
    data->done = false;
    std::this_thread::sleep_for(test->duration());
    int retries = 5;
    while (test->has_enough_samples() == false && retries-- > 0)
    {
        std::this_thread::sleep_for(RETRY_TIME);
    }
    return test->has_enough_samples();
}


float get_sample_rate()
{
#ifdef DUMMY_AUDIO
    return 48000;
#else
    return raspa_get_sampling_rate();
#endif
}

at::TestConfig prepare_base_config(Setup& test_setup, float samplerate)
{
    at::TestConfig config;
    config.channels = test_setup.channels;
    config.delay_samples = test_setup.delay;
    config.length = 0; // Set by each Test
    config.sample_rate = samplerate;
    config.input_gain_db = test_setup.input_gain;
    config.output_gain_db = test_setup.output_gain;
    config.test_level_db = test_setup.test_level;

    for (int i = 0; i < test_setup.channels; i++)
    {
        auto mapping = std::find_if(test_setup.channel_mappings.begin(), test_setup.channel_mappings.end(), [&](const auto& p){return p.first == i;});
        if (mapping != test_setup.channel_mappings.end())
        {
            assert(mapping->second <= test_setup.channels);
            config.channel_mapping.push_back(mapping->second);
            std::cout << "Remapping channel " << mapping->first << " out to channel " << mapping->second << " in" << std::endl;
        }
        else
        {
            config.channel_mapping.push_back(i);
        }
    }
    config.ignored_channels = test_setup.ignored_channels;
    return config;
}

std::vector<std::unique_ptr<at::Test>> create_tests(at::TestConfig base_config, const Setup& setup)
{
    std::vector<std::unique_ptr<at::Test>> tests;
    if (setup.autodetect_delay)
    {
        base_config.length = at::DelayTest::DEFAULT_LENGTH;
        tests.emplace_back(std::make_unique<at::DelayTest>(base_config, 0, at::DelayTest::DEFAULT_LENGTH, setup.buffersize));
    }
    for (const auto& name : setup.tests)
    {
        if (name == "Noise")
        {
            std::cout << "Adding test: Noise floor" << std::endl;
            base_config.length = at::NoiseTest::DEFAULT_LENGTH;
            tests.emplace_back(std::make_unique<at::NoiseTest>(base_config, setup.noise_margin));
        }
        else if (name == "Frequency")
        {
            std::cout << "Adding test: Frequency sweep" << std::endl;
            base_config.length = at::FrequencyTest::DEFAULT_LENGTH;
            tests.emplace_back(std::make_unique<at::FrequencyTest>(base_config, at::FrequencyTest::DEFAULT_LOW,
                at::FrequencyTest::DEFAULT_HIGH, at::FrequencyTest::DEFAULT_THRESHOLD, setup.print_fft));
        }
        if (name == "Level")
        {
            std::cout << "Adding test: Level and crosstalk" << std::endl;
            base_config.length = at::LevelTest::DEFAULT_LENGTH;
            tests.emplace_back(std::make_unique<at::LevelTest>(base_config, setup.level_margin, setup.crosstalk_margin));
        }
        else if (name == "Distortion")
        {
            std::cout << "Adding test: Distortion" << std::endl;
            base_config.length = at::DistortionTest::DEFAULT_LENGTH;
            tests.emplace_back(std::make_unique<at::DistortionTest>(base_config, at::DistortionTest::DEFAULT_THRESHOLD, setup.print_fft));
        }
        else if (name == "Mute")
        {
            std::cout << "Adding test: Mute" << std::endl;
            base_config.length = at::MuteTest::DEFAULT_LENGTH;
            tests.emplace_back(std::make_unique<at::MuteTest>(base_config, setup.mute_level));
        }
    }
    return tests;
}

json create_report(const Setup& config, const std::vector<std::unique_ptr<at::Test>>& tests)
{
    json configuration;
    configuration["channels"] = config.channels;
    configuration["input_gain"] = config.input_gain;
    configuration["output_gain"] = config.output_gain;
    configuration["test_level"] = config.test_level;
    configuration["ignored_channels"] = config.ignored_channels;

    bool all_tests_passed = true;
    json test_reports = json::array();
    for (auto& test : tests)
    {
        auto test_report = test->analyse();
        test_reports.push_back(test_report->details());
        all_tests_passed &= test_report->passed();
    }

    json report;
    report["configuration"] = configuration;
    report["tests"] = test_reports;
    report["all_tests_passed"] = all_tests_passed;
    return report;
}

std::optional<Setup> read_cli(int argc, char** argv)
{
    Setup config;

    argc-=(argc>0); argv+=(argc>0); // skip program name argv[0] if present
    option::Stats  stats(usage, argc, argv);
    std::vector<option::Option> options(stats.options_max);
    std::vector<option::Option> buffer(stats.buffer_max);
    option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);

    if (parse.error())
    {
        return std::nullopt;
    }

    if (options[OPT_IDX_HELP] || argc == 0)
    {
        option::printUsage(std::cout, usage);
        return std::nullopt;
    }

    try
    {
    for (int i=0; i < parse.optionsCount(); i++)
    {
        option::Option& opt = buffer[i];
        switch(opt.index())
        {
            case OPT_IDX_HELP:
            case OPT_IDX_UNKNOWN:
                // should be handled before arriving here
                assert(false);
                break;
            case OPT_IDX_REPORT_FILE:
                {
                    auto path = std::filesystem::path(opt.arg);
                    if (path.filename().empty())
                    {
                        path += "report.json";
                    }
                    config.report_file = opt.arg;
                }
                break;

            case OPT_IDX_TESTS:
                config.tests.push_back(opt.arg);
                break;

            case OPT_IDX_DELAY:
                config.delay = std::stoi(opt.arg);
                break;

            case OPT_IDX_BUFFERSIZE:
                config.buffersize = std::stoi(opt.arg);
                break;

            case OPT_IDX_AUTODETECT_DELAY:
                config.autodetect_delay = true;
                break;

            case OPT_IDX_PRINT_FFT:
                config.print_fft = true;
                break;

            case OPT_IDX_CHANNELS:
                config.channels = std::stoi(opt.arg);
                break;

            case OPT_IDX_INPUT_GAIN:
                config.input_gain = std::stof(opt.arg);
                break;

            case OPT_IDX_OUTPUT_GAIN:
                config.output_gain = std::stof(opt.arg);
                break;

            case OPT_IDX_TEST_LEVEL:
                config.test_level = std::stof(opt.arg);
                break;

            case OPT_IDX_NOISE_FLOOR_MARGIN:
                config.noise_margin = std::stof(opt.arg);
                break;

            case OPT_IDX_CROSSTALK_MARGIN:
                config.crosstalk_margin = std::stof(opt.arg);
                break;

            case OPT_IDX_LEVEL_MARGIN:
                config.level_margin = std::abs(std::stof(opt.arg));
                break;

            case OPT_IDX_CHANNEL_MAPPING:
                config.channel_mappings.push_back({std::stoi(opt.arg), std::stoi(opt.arg + 2)});
                break;

            case OPT_IDX_IGNORE_CHANNEL:
                config.ignored_channels.push_back(std::stoi(opt.arg));
                break;

            case OPT_IDX_MUTE_MARGIN:
                config.mute_level = std::stof(opt.arg);
                break;

            case OPT_IDX_INITIAL_DELAY:
                config.initial_delay = std::chrono::milliseconds(std::stoi(opt.arg));
                break;

            default:
                ProjectTemplateArg::print_error("Unhandled option '", opt, "' \n");
                break;
        }
    }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
        return std::nullopt;
    }
    return config;
}

void run_all_tests(const std::vector<std::unique_ptr<at::Test>>& tests, RealtimeData* realtime_data, bool autodetect_delay, const std::vector<int>& ignored_channels)
{
    for (auto& test : tests)
    {
        if (!run_test(test.get(), realtime_data))
        {
            std::cerr << "Error in test" << std::endl;
            return;
        }
        if (autodetect_delay && test.get() == tests.front().get())
        {
            auto report = test->analyse();
            auto* delay_report = static_cast<at::DelayTest::DelayReport*>(report.get());
            int ref_channel = 0;
            for (int ch = 0; ch < test->channels(); ++ch)
            {
                if (std::find(ignored_channels.begin(), ignored_channels.end(), ch) == ignored_channels.end())
                {
                    ref_channel = ch;
                    break;
                }
            }
            int delay = delay_report->delay_samples(ref_channel);
            std::cout << "Autodetected delay: " << delay << std::endl;
            for (auto& t : tests)
            {
                t->set_delay(delay);
            }
        }
        std::this_thread::sleep_for(TEST_WAIT_TIME);
    }
}

int main(int argc, char* argv[])
{
    auto setup = read_cli(argc, argv);
    if (!setup)
    {
        std::cerr << "Error reading config" << std::endl;
        return -1;
    }

    // TODO - start audio / raspa

    RealtimeData realtime_data;
    realtime_data.buffersize = setup->buffersize;
    realtime_data.channels = setup->channels;
    realtime_data.current_test = nullptr;
    realtime_data.done = true;

#ifdef DUMMY_AUDIO
    int res = init_dummy_audio(&realtime_data, setup->buffersize);
#else
    int res = raspa_init();
    if (res == 0)
    {
        res = raspa_open(setup->buffersize, audio_callback, &realtime_data, 0);
    }
    if (res != 0)
    {
        std::cerr << "Error initializing raspa: " << raspa_get_error_msg(-res) << std::endl;
        return -1;
    }
    raspa_start_realtime();
#endif

    auto test_config = prepare_base_config(setup.value(), get_sample_rate());
    auto tests = create_tests(test_config, setup.value());
    if (tests.empty())
    {
        std::cerr << "No tests configured" << std::endl;
        return 0;
    }

#ifndef DUMMY_AUDIO
    // Give relays time to switch. The input cap is large so we need a long time to get rid of all DC offset
    std::this_thread::sleep_for(setup->initial_delay);
#endif

    run_all_tests(tests, &realtime_data, setup->autodetect_delay, test_config.ignored_channels);

    json report = create_report(setup.value(), tests);

    std::ofstream o(setup->report_file);
    o << std::setw(4) << report << std::endl;

#ifdef DUMMY_AUDIO
    dummy_tread_flag = false;
    dummy_thread.join();
#else
    raspa_close();
#endif
    if (report["all_tests_passed"])
    {
        std::cout << "All tests passed" << std::endl;
        return 0;
    }
    std::cout << "One or more tests failed" << std::endl;
    return  -1;
}

