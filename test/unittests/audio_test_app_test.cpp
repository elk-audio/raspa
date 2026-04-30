#include "gtest/gtest.h"

#include "audio_test.cpp"

// A simple test case
TEST (SampleTest, SimpleTestCase) {
    ASSERT_TRUE (1);
}

class BaseClassTestCase;

class TestTest : public at::Test
{
    public:
    TestTest(const at::TestConfig& config) : at::Test(config)
    {

    }
    bool passed()
    {
        return true;
    }

    std::unique_ptr<at::Report> analyse() override
    {
        return nullptr;
    }
    friend class BaseClassTestCase;
};

TEST(AnalysisTest, XCorrTest)
{
    std::vector<float> data = {1, 2, 3, 2, 1};
    auto acorr = at::xcorr(data, data);

    EXPECT_EQ(10, acorr.size());
    EXPECT_EQ(1,  acorr[0]);
    EXPECT_EQ(4,  acorr[1]);
    EXPECT_EQ(10, acorr[2]);
    EXPECT_EQ(16, acorr[3]);
    EXPECT_EQ(19, acorr[4]);
    EXPECT_EQ(16, acorr[5]);
    EXPECT_EQ(10, acorr[6]);
    EXPECT_EQ(4,  acorr[7]);
    EXPECT_EQ(1,  acorr[8]);
}

class BaseClassTestCase : public ::testing::Test
{
    protected:
    BaseClassTestCase()
    {
    }
    void SetUp()
    {
        at::TestConfig config;
        config.channels = 2;
        config.length = 256;
        config.delay_samples = 10;
        config.channel_mapping = {0,1};
        _test_module = std::make_unique<TestTest>(config);
    }

    void TearDown()
    {
    }

    std::unique_ptr<TestTest> _test_module;
};

TEST_F(BaseClassTestCase, FillTest)
{
    // Fill with different sized buffers to test chunked data recording
    for (int i = 1; i > 0; i++)
    {
        std::vector<float> data(i * 2, static_cast<float>(i)); // 2 channels, planar
        _test_module->record_audio(data.data(), i);

        if (_test_module->has_enough_samples())
        {
            break;
        }
    }
    // TODO - check size of internal buffers
}

class NoiseTestCase : public ::testing::Test
{
protected:
    NoiseTestCase()
    {
    }
    static constexpr int TEST_LENGTH = 256;

    void SetUp()
    {
        at::TestConfig config;
        config.channels = 2;
        config.length = TEST_LENGTH;
        config.delay_samples = 0;
        config.channel_mapping = {0,1};
        _test_module = std::make_unique<at::NoiseTest>(config, -10);
    }

    void TearDown()
    {
    }

    std::unique_ptr<at::NoiseTest> _test_module;
};

constexpr float RMS_MARGIN = 0.01;

TEST_F (NoiseTestCase, SquareTest)
{
    std::vector<float> square(TEST_LENGTH * 2, 0.5f); // 2 channels, planar

    _test_module->record_audio(square.data(), TEST_LENGTH);
    ASSERT_TRUE (_test_module->has_enough_samples());
    auto report = _test_module->analyse();
    ASSERT_TRUE(report);
    EXPECT_FALSE(report->passed());

    auto noise_report = static_cast<at::NoiseTest::NoiseReport*>(report.get());
    EXPECT_NEAR(-6.02, noise_report->noise_level(0), RMS_MARGIN);
    EXPECT_NEAR(-6.02, noise_report->noise_level(1), RMS_MARGIN);
}

TEST_F (NoiseTestCase, SineTest)
{
    std::vector<float> sine(TEST_LENGTH * 2);
    std::generate(sine.begin(), sine.begin() + TEST_LENGTH, [n = 0] () mutable { return std::sin(n++) * 0.1f; });
    std::copy(sine.begin(), sine.begin() + TEST_LENGTH, sine.begin() + TEST_LENGTH);

    _test_module->record_audio(sine.data(), TEST_LENGTH);
    ASSERT_TRUE (_test_module->has_enough_samples());
    auto report = _test_module->analyse();
    ASSERT_TRUE(report);
    EXPECT_TRUE(report->passed());

    auto noise_report = static_cast<at::NoiseTest::NoiseReport*>(report.get());
    EXPECT_NEAR(-23.02, noise_report->noise_level(0), RMS_MARGIN);
    EXPECT_NEAR(-23.02, noise_report->noise_level(1), RMS_MARGIN);
}

class FrequencyTestCase : public ::testing::Test
{
protected:
    static constexpr int TEST_LENGTH = 2048;

    FrequencyTestCase()
    {
    }
    void SetUp()
    {
        at::TestConfig config;
        config.channels = 2;
        config.length = TEST_LENGTH;
        config.delay_samples = 0;
        config.sample_rate = 48000;
        config.channel_mapping = {0,1};
        _test_module = std::make_unique<at::FrequencyTest>(config, 200, 20000, 3, false);
    }

    void TearDown()
    {
    }

    std::unique_ptr<at::FrequencyTest> _test_module;
};


TEST_F (FrequencyTestCase, LoopbackTest)
{
    std::array<float, TEST_LENGTH * 2> data;

    _test_module->generate_audio_to(data.data(), TEST_LENGTH);
    _test_module->record_audio(data.data(), TEST_LENGTH);

    //for (const auto& sample : data[0]) { std::cout << sample << ", "; }
    std::cout << std::endl;

    ASSERT_TRUE (_test_module->has_enough_samples());
    auto report = _test_module->analyse();
    EXPECT_TRUE(report);
    EXPECT_TRUE(report->passed());

    auto freq_report = static_cast<at::FrequencyTest::FrequencyReport*>(report.get());
    auto ch = freq_report->frequency_response(0);
}

class DelayTestCase : public ::testing::Test
{
protected:
    static constexpr int TEST_LENGTH = 256;
    static constexpr int EXPECTED_DELAY = 50;
    static constexpr int BLOCKSIZE = 32;
    static constexpr int TOLERANCE = 2;

    void SetUp()
    {
        at::TestConfig config;
        config.channels = 2;
        config.length = TEST_LENGTH;
        config.delay_samples = 0;
        config.channel_mapping = {0,1};
        _test_module = std::make_unique<at::DelayTest>(config, EXPECTED_DELAY, TOLERANCE, BLOCKSIZE);
    }

    std::unique_ptr<at::DelayTest> _test_module;
};

TEST_F(DelayTestCase, ImpulseDelayTest)
{
    std::array<float, TEST_LENGTH * 2> data{};
    _test_module->generate_audio_to(data.data(), TEST_LENGTH);
    int shift = 119;

    for (int ch = 0; ch < 2; ++ch)
    {
        std::rotate(data.begin(), data.begin() + shift, data.begin() + TEST_LENGTH);
        std::rotate(data.begin() + TEST_LENGTH, data.begin() + shift + TEST_LENGTH, data.end());
    }

    _test_module->record_audio(data.data(), TEST_LENGTH);

    ASSERT_TRUE(_test_module->has_enough_samples());
    auto report = _test_module->analyse();
    ASSERT_TRUE(report);
    EXPECT_TRUE(report->passed());

    auto delay_report = static_cast<at::DelayTest::DelayReport*>(report.get());
    EXPECT_EQ(EXPECTED_DELAY, delay_report->delay_samples(0));
    EXPECT_EQ(EXPECTED_DELAY, delay_report->delay_samples(1));
}

TEST_F(DelayTestCase, IgnoredChannelDoesNotFail)
{
    std::array<float, TEST_LENGTH * 2> data{};
    _test_module->generate_audio_to(data.data(), TEST_LENGTH);

    // shift=119 applied twice moves the impulse to position 18, which resolves to delay=50
    int shift = 119;
    for (int ch = 0; ch < 2; ++ch)
    {
        std::rotate(data.begin(), data.begin() + shift, data.begin() + TEST_LENGTH);
        std::rotate(data.begin() + TEST_LENGTH, data.begin() + shift + TEST_LENGTH, data.end());
    }

    // Silence channel 0 to simulate a broken/disconnected input
    std::fill(data.begin(), data.begin() + TEST_LENGTH, 0.0f);

    at::TestConfig config;
    config.channels = 2;
    config.length = TEST_LENGTH;
    config.delay_samples = 0;
    config.channel_mapping = {0, 1};
    config.ignored_channels = {0};
    at::DelayTest test_ignored(config, EXPECTED_DELAY, TOLERANCE, BLOCKSIZE);

    test_ignored.generate_audio_to(data.data(), TEST_LENGTH);
    for (int ch = 0; ch < 2; ++ch)
    {
        std::rotate(data.begin(), data.begin() + shift, data.begin() + TEST_LENGTH);
        std::rotate(data.begin() + TEST_LENGTH, data.begin() + shift + TEST_LENGTH, data.end());
    }
    std::fill(data.begin(), data.begin() + TEST_LENGTH, 0.0f);

    test_ignored.record_audio(data.data(), TEST_LENGTH);
    ASSERT_TRUE(test_ignored.has_enough_samples());
    auto report = test_ignored.analyse();
    ASSERT_TRUE(report);
    EXPECT_TRUE(report->passed());

    auto delay_report = static_cast<at::DelayTest::DelayReport*>(report.get());
    EXPECT_EQ(EXPECTED_DELAY, delay_report->delay_samples(1));
}

class LevelTestCase : public ::testing::Test
{
protected:
    static constexpr int TEST_LENGTH = 1024;

    void SetUp()
    {
        at::TestConfig config;
        config.channels = 2;
        config.length = TEST_LENGTH;
        config.delay_samples = 0;
        config.sample_rate = 48000;
        config.channel_mapping = {0,1};
        _test_module = std::make_unique<at::LevelTest>(config, 1.0f, -40.0f);
    }

    std::unique_ptr<at::LevelTest> _test_module;
};

TEST_F(LevelTestCase, LoopbackTest)
{
    std::array<float, TEST_LENGTH * 2> data{};
    _test_module->generate_audio_to(data.data(), TEST_LENGTH);
    _test_module->record_audio(data.data(), TEST_LENGTH);

    ASSERT_TRUE(_test_module->has_enough_samples());
    auto report = _test_module->analyse();
    ASSERT_TRUE(report);
    EXPECT_TRUE(report->passed());

    auto bal = static_cast<at::LevelTest::LevelReport*>(report.get());
    EXPECT_NEAR(bal->level_db(0), bal->level_db(1), 1.0f);
}

class MuteTestCase : public ::testing::Test
{
protected:
    static constexpr int TEST_LENGTH = 1024;

    void SetUp()
    {
        at::TestConfig config;
        config.channels = 2;
        config.length = TEST_LENGTH;
        config.delay_samples = 0;
        config.sample_rate = 48000;
        config.channel_mapping = {0, 1};
        _test_module = std::make_unique<at::MuteTest>(config, -50.0f);
    }

    std::unique_ptr<at::MuteTest> _test_module;
};

TEST_F(MuteTestCase, SilenceTest)
{
    std::array<float, TEST_LENGTH * 2> data{};
    _test_module->record_audio(data.data(), TEST_LENGTH);

    ASSERT_TRUE(_test_module->has_enough_samples());
    auto report = _test_module->analyse();
    ASSERT_TRUE(report);
    EXPECT_TRUE(report->passed());

    auto mute_report = static_cast<at::MuteTest::MuteReport*>(report.get());
    EXPECT_NEAR(-120.0f, mute_report->level_db(0), 1.0f);
    EXPECT_NEAR(-120.0f, mute_report->level_db(1), 1.0f);
}

TEST_F(MuteTestCase, ToneTest)
{
    std::array<float, TEST_LENGTH * 2> data{};
    _test_module->generate_audio_to(data.data(), TEST_LENGTH);
    _test_module->record_audio(data.data(), TEST_LENGTH);

    ASSERT_TRUE(_test_module->has_enough_samples());
    auto report = _test_module->analyse();
    ASSERT_TRUE(report);
    EXPECT_FALSE(report->passed());

    auto mute_report = static_cast<at::MuteTest::MuteReport*>(report.get());
    EXPECT_GT(mute_report->level_db(0), -50.0f);
    EXPECT_GT(mute_report->level_db(1), -50.0f);
}

// Tests for --ignore-channel: uses NoiseTest with 2 channels where channel 0 is loud
// (square wave at 0.5 → ~-6 dB, above the -10 dB threshold) and channel 1 is silent.
class IgnoreChannelTestCase : public ::testing::Test
{
protected:
    static constexpr int TEST_LENGTH = 256;
    static constexpr float NOISE_THRESHOLD_DB = -10.0f;

    void SetUp()
    {
        _base_config.channels = 2;
        _base_config.length = TEST_LENGTH;
        _base_config.delay_samples = 0;
        _base_config.channel_mapping = {0, 1};

        _data.assign(TEST_LENGTH * 2, 0.0f);
        std::fill(_data.begin(), _data.begin() + TEST_LENGTH, 0.5f); // ch 0: loud square wave
        // ch 1: silence (stays 0.0)
    }

    at::TestConfig _base_config;
    std::vector<float> _data;
};

TEST_F(IgnoreChannelTestCase, FailsWithoutIgnore)
{
    at::NoiseTest test(_base_config, NOISE_THRESHOLD_DB);
    test.record_audio(_data.data(), TEST_LENGTH);
    ASSERT_TRUE(test.has_enough_samples());
    auto report = test.analyse();
    ASSERT_TRUE(report);
    EXPECT_FALSE(report->passed());
    EXPECT_TRUE(report->details()["ignored_channels"].empty());
}

TEST_F(IgnoreChannelTestCase, PassesWhenFailingChannelIgnored)
{
    auto config = _base_config;
    config.ignored_channels = {0};
    at::NoiseTest test(config, NOISE_THRESHOLD_DB);
    test.record_audio(_data.data(), TEST_LENGTH);
    ASSERT_TRUE(test.has_enough_samples());
    auto report = test.analyse();
    ASSERT_TRUE(report);
    EXPECT_TRUE(report->passed());
    EXPECT_EQ(report->details()["ignored_channels"], json::array({0}));
}

TEST_F(IgnoreChannelTestCase, StillFailsWhenOnlyPassingChannelIgnored)
{
    auto config = _base_config;
    config.ignored_channels = {1}; // ignore silent channel, not the loud one
    at::NoiseTest test(config, NOISE_THRESHOLD_DB);
    test.record_audio(_data.data(), TEST_LENGTH);
    ASSERT_TRUE(test.has_enough_samples());
    auto report = test.analyse();
    ASSERT_TRUE(report);
    EXPECT_FALSE(report->passed());
}
