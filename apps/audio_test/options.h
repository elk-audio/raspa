#include <cstdio> 
#include "optionparser.h"

constexpr auto UsageString =
    "\nCommand-line tool for running automated audio hardware tests on a loopback connection"
    " (output wired back to input). Tests generate stimulus signals, record the response, "
    "and produces a JSON report with per-channel pass/fail results."
    "The app returns 0 if all tests pass successfully and -1 otherwise, this makes it easy "
    "to integrate in a bash/python script. The resulting report file can be plotted using"
    " the included plot_report.py script.\n\n"

    "The following tests are available:\r\n"
    "Delay - Measures round trip delay. Sends an impulse and cross-correlates the recorded"
    " signal to measure the round-trip delay in samples. Reports the measured"
    " delay and whether it falls within a given tolerance.\n\n"
    "Noise  - Measures noise floor. Records silence and computes the RMS noise floor"
    " per channel in dBFS. Reports pass/fail against a configurable threshold.\n\n"
    "Frequency - Tests frequency response. Generates a log sine sweep and analyses"
    " the frequency response per channel via FFT. Reports the flatness (min/max dB within"
    " the sweep band) and pass/fail against a margin.\n\n"
    "Level - Tests levels and crosstalk. Plays a 1 kHz windowed sine on each output channel"
    " tin turn and measures the level on all inputs. Reports channel balance and crosstalk in dB.\n\n"
    "Distortion - Tests harmonic distortion. Generates a windowed sine on all channels"
    " simultaneously and analyses the FFT of each recorded channel. Reports pass/fail"
    " based on whether all spectral components outside the fundamental are at least a"
    " given threshold tbelow the peak.\n\n"
    "Mute - Tests that signal is muted. Generates a windowed sine on each output channel"
    " in turn and measures the recorded level. Reports the recorder rms level in dB and"
    " pass/fail whether it is below the given margin.\n\n"
    "Options:\r\n";


////////////////////////////////////////////////////////////////////////////////
// Helpers for optionparse
////////////////////////////////////////////////////////////////////////////////

struct ProjectTemplateArg : public option::Arg
{

    static void print_error(const char* msg1, const option::Option& opt, const char* msg2)
    {
        fprintf(stderr, "%s", msg1);
        fwrite(opt.name, opt.namelen, 1, stderr);
        fprintf(stderr, "%s", msg2);
    }

    static option::ArgStatus Unknown(const option::Option& option, bool msg)
    {
        if (msg)
        {
            print_error("Unknown option '", option, "'\n");
        }
        return option::ARG_ILLEGAL;
    }

    static option::ArgStatus NonEmpty(const option::Option& option, bool msg)
    {
        if (option.arg != 0 && option.arg[0] != 0)
        {
            return option::ARG_OK;
        }

        if (msg)
        {
            print_error("Option '", option, "' requires a non-empty argument\n");
        }
        return option::ARG_ILLEGAL;
    }

    static option::ArgStatus Numeric(const option::Option& option, bool msg)
    {
        char* endptr = 0;
        if (option.arg != 0 && strtol(option.arg, &endptr, 10))
        {};

        if (endptr != option.arg && *endptr == 0)
        {
          return option::ARG_OK;
        }
        if (msg)
        {
            print_error("Option '", option, "' requires a numeric argument\n");
        }
        return option::ARG_ILLEGAL;
    }

};

////////////////////////////////////////////////////////////////////////////////
// Command Line options descriptors
////////////////////////////////////////////////////////////////////////////////

// List here the different command line options
enum OptionIndex
{ 
    OPT_IDX_UNKNOWN,
    OPT_IDX_HELP,
    OPT_IDX_REPORT_FILE,
    OPT_IDX_TESTS,
    OPT_IDX_DELAY,
    OPT_IDX_BUFFERSIZE,
    OPT_IDX_AUTODETECT_DELAY,
    OPT_IDX_PRINT_FFT,
    OPT_IDX_CHANNELS,
    OPT_IDX_INPUT_GAIN,
    OPT_IDX_OUTPUT_GAIN,
    OPT_IDX_TEST_LEVEL,
    OPT_IDX_NOISE_FLOOR_MARGIN,
    OPT_IDX_CROSSTALK_MARGIN,
    OPT_IDX_LEVEL_MARGIN,
    OPT_IDX_CHANNEL_MAPPING,
    OPT_IDX_IGNORE_CHANNEL,
    OPT_IDX_MUTE_MARGIN,
    OPT_IDX_INITIAL_DELAY,
};

// Option types (UNUSED is generally used for options that take a value as argument)
enum OptionType
{
    OPT_TYPE_UNUSED = 0,
    OPT_TYPE_DISABLED = 1,
    OPT_TYPE_ENABLED = 2
};

// Option descriptors, one for each entry of the OptionIndex enum
const option::Descriptor usage[] =
{
    {
        OPT_IDX_UNKNOWN,    // index
        OPT_TYPE_UNUSED,    // type
        "",         // shortopt
        "",         // longopt
        ProjectTemplateArg::Unknown, // check_arg
        UsageString // help
    },
    {
        OPT_IDX_HELP,
        OPT_TYPE_UNUSED,
        "h?",
        "help",
        ProjectTemplateArg::None,
        "\t\t-h --help \tPrint usage and exit."
    },
    {
        OPT_IDX_REPORT_FILE,
        OPT_TYPE_UNUSED,
        "r",
        "report-file",
        ProjectTemplateArg::NonEmpty,
        "\t\t-r <path>, --report-file=<path> \tPath to report file written after tests [default= ./ ]."
    },
    {
        OPT_IDX_TESTS,
        OPT_TYPE_UNUSED,
        "t",
        "test",
        ProjectTemplateArg::NonEmpty,
        "\t\t-t <Test>, --test=<Test> \t Tests to run. Valid values are Delay, Noise, Frequency, Level, Distortion. Use multiple times to run several tests"
    },
    {
        OPT_IDX_DELAY,
        OPT_TYPE_UNUSED,
        "d",
        "delay",
        ProjectTemplateArg::Numeric,
        "\t\t-d <Delay>, --delay=<Delay> \t Delay between input and output in samples."
    },
    {
        OPT_IDX_BUFFERSIZE,
        OPT_TYPE_UNUSED,
        "b",
        "buffersize",
        ProjectTemplateArg::Numeric,
        "\t\t-b <size>, --buffersize=<Size> \t Audio processing blocksize [default=64}."
    },
    {
        OPT_IDX_AUTODETECT_DELAY,
        OPT_TYPE_UNUSED,
        "a",
        "detect-delay",
        ProjectTemplateArg::None,
        "\t\t-a , --autodetect-delay \t Runs DelayTest first to detect the delay between input and output and uses that for all subsequent tests"
    },
   {
        OPT_IDX_PRINT_FFT,
        OPT_TYPE_UNUSED,
        "",
        "fft",
        ProjectTemplateArg::None,
        "\t\t, --fft \t Prints the calculated frequency response curve to the FrequencyTest report"
    },
    {
        OPT_IDX_CHANNELS,
        OPT_TYPE_UNUSED,
        "c",
        "channels",
        ProjectTemplateArg::NonEmpty,
        "\t\t-c <number of channels>, --channels=<number> \tThe number of audio channels to use in the test [default=2]."
    },
    {
        OPT_IDX_INPUT_GAIN,
        OPT_TYPE_UNUSED,
        "i",
        "input-gain",
        ProjectTemplateArg::NonEmpty,
        "\t\t-i <gain>, --input-gain=<gain> \tInput gain of system in dB. Use to compensate if loopback connection does not have equal gain [default=dB0]."
    },
    {
        OPT_IDX_OUTPUT_GAIN,
        OPT_TYPE_UNUSED,
        "o",
        "output-gain",
        ProjectTemplateArg::NonEmpty,
        "\t\t-i <gain>, --input-gain=<gain> \tOutput gain of system in dB. Use to compensate if loopback connection does not have equal gain [default=0dB]."
    },
    {
        OPT_IDX_TEST_LEVEL,
        OPT_TYPE_UNUSED,
        "l",
        "test-level",
        ProjectTemplateArg::NonEmpty,
        "\t\t-l <level>, --test-level=<level> \tThe nominal level of test signals. Should be set to avoid clipping[default=-3dB]."
    },
    {
        OPT_IDX_NOISE_FLOOR_MARGIN,
        OPT_TYPE_UNUSED,
        "",
        "noise-margin",
        ProjectTemplateArg::NonEmpty,
        "\t\t --noise-margin=<margin> \tThe accepted noise floor level in dB. A recorded noise floor higher than the given value on any channel will result in a failed test [default=-90dB]."
    },
    {
        OPT_IDX_CROSSTALK_MARGIN,
        OPT_TYPE_UNUSED,
        "l",
        "crosstalk-margin",
        ProjectTemplateArg::NonEmpty,
        "\t\t --crosstalk-margin=<margin> \tThe accepted crosstalk level in dB. A detected cross talk higher than the give value on any channel will result in a failed test [default=-80dB]."
    },
    {
        OPT_IDX_LEVEL_MARGIN,
        OPT_TYPE_UNUSED,
        "l",
        "level-margin",
        ProjectTemplateArg::NonEmpty,
        "\t\t --level-margin=<margin> \tThe accepted margin in level detection in dB. A recorded signal that deviates more than the give value on any channel will result in a failed test [default=1dB]."
    },
    {
        OPT_IDX_MUTE_MARGIN,
        OPT_TYPE_UNUSED,
        "",
        "mute-margin",
        ProjectTemplateArg::NonEmpty,
        "\t\t --mute-margin=<margin> \tThe accepted margin in level detection in dB for the mute test. A peak above this value on any channel will result in a failed test [default=-60dB]."
    },
    {
        OPT_IDX_CHANNEL_MAPPING,
        OPT_TYPE_UNUSED,
        "m",
        "channel-mapping",
        ProjectTemplateArg::NonEmpty,
        "\t\t-m <mapping rule>, --channel-mapping=<mapping rule> \tRemaps the channel layout if i.e output 0 is not connected to input 0. A rule should have the format n:m where 1:2 means input 1 is connected to output 2. Use the argument multiple times to map multiple channels"
    },
    {
        OPT_IDX_IGNORE_CHANNEL,
        OPT_TYPE_UNUSED,
        "",
        "ignore-channel",
        ProjectTemplateArg::Numeric,
        "\t\t --ignore-channel=<n> \tIgnore test failures on channel n. Use multiple times to ignore multiple channels."
    },
    {
        OPT_IDX_INITIAL_DELAY,
        OPT_TYPE_UNUSED,
        "",
        "initial-delay",
        ProjectTemplateArg::Numeric,
        "\t\t --initial-delay=<ms> \tDelay in milliseconds to wait before starting any tests. [default=3000ms]"
    },
    // Don't touch this one (set default values for optionparse library)
    { 0, 0, 0, 0, 0, 0}
};
