#include <iostream>
#include "signal_recorder.h"
#include "raspa/raspa.h"
#include <csignal>

constexpr int DEFAULT_FREQ = 440;
constexpr float DEFAULT_AMPLITUDE = 0.7;
constexpr int DEFAULT_NUM_FRAMES = 64;
constexpr int DEFAULT_NUM_BUFFERS_TO_RECORD = 2000;
#define DEFAULT_REC_PATH "/udata"

enum class OPTION_STATE
{
    NONE,
    RECV_INPUT_CHAN,
    RECV_OUTPUT_CHAN,
    RECV_FREQ,
    RECV_NUM_BUFFERS,
    RECV_AMPLITUDE,
    RECV_BUFFER_SIZE,
};

bool stop_program_flag = false;

void sigint_handler(int __attribute__((unused)) sig)
{
    stop_program_flag = true;
}

void print_usage()
{
    std::cout << "Testing program which records input signal of any channel of the"
                 " board for into a wav file, while sending a tone on any"
                 " output channels at the same time. Useful as a test program" 
                 " to quickly verify input and output audio channels work\n\n"
                 "Note that you cannot set the names of the channel recordings!\n"
                 "They will always follow the prefix chan_<num>_recording.wav and will"
                 " be overwritten every session\n\n";


    std::cout << "Usage\n\n";
    std::cout << " signal_recorder [-h] [-i] [-o] [-f]\n\n";
    std::cout << "Options:\n";
    std::cout << "    -h             : Help for usage options.\n";
    std::cout << "    -i  : Specify a list of input channels to be recorded. Default is all.\n";
    std::cout << "    -o  : Specify a list of output channels from which tone is generated. Default is all\n";
    std::cout << "    -f  : Specify the output tone frequency. Default is 440Hz\n";
    std::cout << "    -a  : amplitude of the tone generated. between 0 - 1\n ";
    std::cout << "    -n  : num audio buffers to record\n ";
    std::cout << "    -b  : The Rt audio buffer size\n ";
}

int main(int argc, char *argv[])
{
    std::cout << "\n\n#########################\n";
    std::cout << "Signal recording program.\n";
    std::cout << "#########################\n\n";

    signal(SIGINT, sigint_handler);

    std::vector<int> output_chan_list;
    std::vector<int> input_chan_list;
    int output_freq = DEFAULT_FREQ;
    float amplitude = DEFAULT_AMPLITUDE;
    int num_buffers_to_record = DEFAULT_NUM_BUFFERS_TO_RECORD;
    int num_frames = DEFAULT_NUM_FRAMES;
    std::string recording_path = DEFAULT_REC_PATH;

    int num_input_chans = 0;
    int num_output_chans = 0;

    // Poor mans parser. TODO : Use a library god dammit
    if (argc < 2)
    {
        std::cout << "-> Default mode selected : Recording all input channels\n";
        std::cout << "-> For help use signal_recorder [-h]\n\n";
    }
    else
    {
        OPTION_STATE option_state = OPTION_STATE::NONE;

        for(int i = 1; i < argc; i++)
        {
            if(std::strcmp(argv[i], "-h") == 0)
            {
                option_state = OPTION_STATE::NONE;
                print_usage();
                exit(0);
            }
            else if(std::strcmp(argv[i], "-i") == 0)
            {
                option_state = OPTION_STATE::RECV_INPUT_CHAN;
            }
            else if(std::strcmp(argv[i], "-o") == 0)
            {
                option_state = OPTION_STATE::RECV_OUTPUT_CHAN;
            }
            else if(std::strcmp(argv[i], "-f") == 0)
            {
                option_state = OPTION_STATE::RECV_FREQ;
            }
            else if(std::strcmp(argv[i], "-a") == 0)
            {
                option_state = OPTION_STATE::RECV_AMPLITUDE;
            }
            else if(std::strcmp(argv[i], "-n") == 0)
            {
                option_state = OPTION_STATE::RECV_NUM_BUFFERS;
            }
            else if(std::strcmp(argv[i], "-b") == 0)
            {
                option_state = OPTION_STATE::RECV_BUFFER_SIZE;
            }
            else if(option_state == OPTION_STATE::NONE)
            {
                std::cout << "Error : Unknown option " << argv[i] << "\n\n";
                exit(0);
            }
            else if(option_state == OPTION_STATE::RECV_INPUT_CHAN)
            {
                auto chan_num = atoi(argv[i]);
                input_chan_list.push_back(chan_num);
            }
            else if(option_state == OPTION_STATE::RECV_OUTPUT_CHAN)
            {
                auto chan_num = atoi(argv[i]);
                output_chan_list.push_back(chan_num);
            }
            else if(option_state == OPTION_STATE::RECV_FREQ)
            {
                output_freq = atoi(argv[i]);
            }
            else if(option_state == OPTION_STATE::RECV_AMPLITUDE)
            {
                amplitude = atof(argv[i]);
            }
            else if(option_state == OPTION_STATE::RECV_NUM_BUFFERS)
            {
                num_buffers_to_record = atoi(argv[i]);
            }
            else if(option_state == OPTION_STATE::RECV_BUFFER_SIZE)
            {
                num_frames = atoi(argv[i]);
            }
        }
    }

    std::cout << "Session details:\n";
    std::cout << "-> Output tone Freq " << output_freq << std::endl;
    std::cout << "-> Output amplitude " << amplitude << std::endl;
    std::cout << "-> Num buffers to record " << num_buffers_to_record << std::endl;
    std::cout << "-> Path to files : " << recording_path << std::endl;


    SignalRecorder signal_recorder;

    // initialize raspa
    auto res = raspa_init();
    if (res < 0)
    {
        std::cout <<"-> Error initializing RASPA: " << raspa_get_error_msg(-res) << std::endl;
        exit(res);
    }

    res = raspa_open(num_frames, &SignalRecorder::rt_process_callback, &signal_recorder, 0);
    if (res < 0)
    {
        fprintf(stderr, "-> Error opening device: %s\n", raspa_get_error_msg(-res));
        exit(res);
    }

    num_input_chans = raspa_get_num_input_channels();
    num_output_chans = raspa_get_num_output_channels();


    // Default input chan list
    if (input_chan_list.size() == 0)
    {
        for(int i = 0; i < num_input_chans; i++)
        {
                input_chan_list.push_back(i);
        }
    }
    else
    {
        // Check if input chan list exceeds total number of input channels
        for(auto chan : input_chan_list)
        {
            if(chan > num_input_chans)
            {
                std::cout << "-> Error : Input Channel " << chan << " does not exist\n\n";
                exit(0);
            }
        }
    }

    if (output_chan_list.size() == 0)
    {
        // Default output chan list
        for(int i = 0; i < num_output_chans; i++)
        {
                output_chan_list.push_back(i);
        }
    }
    else
    {
        // Check if output chan list exceeds total number of input channels
        for(auto chan : output_chan_list)
        {
            if(chan > num_output_chans)
            {
                std::cout << "-> Error : Output Channel " << chan << " does not exist\n\n";
                exit(0);
            }
        }
    }

    std::cout << "-> Recording Input Channels ";
    for(auto chan : input_chan_list)
    {
        std::cout << chan << ", ";
    }
    std::cout << "\n";

    std::cout << "-> Generating tone on output channels  ";
    for(auto chan : output_chan_list)
    {
        std::cout << chan << ", ";
    }
    std::cout << "\n";

    signal_recorder.init(input_chan_list,
                        output_chan_list,
                        output_freq,
                        amplitude,
                        num_buffers_to_record,
                        raspa_get_num_input_channels(),
                        raspa_get_num_output_channels(),
                        raspa_get_sampling_rate(),
                        num_frames,
                        recording_path);

    std::cout << "\nStarting ...\n";
    raspa_start_realtime();

    while(!signal_recorder.get_done_recording() && !stop_program_flag)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    raspa_close();

    signal_recorder.write_recording_to_files();

    std::cout << "-> Done! Exiting..\n";
}