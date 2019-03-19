#include <portaudio.h>
#include <cstdio>
#include <csignal>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>

typedef int16_t SAMPLE;
#define SAMPLE_RATE  (16000)
#define NUM_CHANNELS (1)
#define SAMPLE_SILENCE  (0)

int g_finished = paContinue;

static int recordCallback( const void *inputBuffer, void *outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData )
{
    auto stream= (Aws::IOStream*)userData;
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;

    if( inputBuffer == nullptr )
    {
        for( unsigned long i=0; i<framesPerBuffer; i++ )
        {
            SAMPLE sample = SAMPLE_SILENCE;
            stream->write((char*)&sample, sizeof(SAMPLE));
            if (NUM_CHANNELS == 2) {
                stream->write((char*)&sample, sizeof(SAMPLE));
            }
            else {
            }
            printf("got here \t ");
        }
    }
    else
    {
        for( unsigned long i=0; i < framesPerBuffer; i++ )
        {
            SAMPLE sample = static_cast<SAMPLE>(*rptr++);
            stream->write((char*)&sample, sizeof(SAMPLE));
            if (NUM_CHANNELS == 2) {
                sample = static_cast<SAMPLE>(*rptr++);
                stream->write((char*)&sample, sizeof(SAMPLE));
            }
        }
    }
    return g_finished;
}

void intHandler(int) {
    g_finished = paComplete;
}

int StartRecording(Aws::IOStream& targetStream)
{

    signal(SIGINT, intHandler);
    PaStreamParameters  inputParameters;
    PaStream*           stream;
    PaError             err = paNoError;

    err = Pa_Initialize();
    if( err != paNoError ) {
        fprintf(stderr,"Error: Failed to initialize PortAudio.\n");
        return -1;
    }

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        Pa_Terminate();
        return -1;
    }
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = paInt16;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultHighInputLatency;
    // inputParameters.suggestedLatency = 0.5;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
            &stream,
            &inputParameters,
            nullptr,  /* &outputParameters, */
            SAMPLE_RATE,
            paFramesPerBufferUnspecified, // FRAMES_PER_BUFFER
            paClipOff,      /* we won't output out of range samples so don't bother clipping them */
            recordCallback,
            &targetStream);

    if( err != paNoError ) {
        fprintf(stderr, "Failed to open stream.\n");
        goto done;
    }

    err = Pa_StartStream( stream );
    if( err != paNoError ) {
        fprintf(stderr, "Failed to start stream.\n");
        goto done;
    }
    printf("\n=== Now recording!! Speak into the microphone. ===\n"); fflush(stdout);

    while( ( err = Pa_IsStreamActive( stream ) ) == 1 )
    {
        Pa_Sleep(1000); // TODO: use thread signals rather than busy loop
    }
    if( err < 0 ) goto done;

    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto done;

done:
    Pa_Terminate();
    return 0;
}
