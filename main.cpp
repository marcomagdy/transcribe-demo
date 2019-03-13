#include <aws/core/Aws.h>
#include <aws/transcribestreaming/TranscribeStreamingServiceClient.h>
#include <aws/transcribestreaming/model/StartStreamTranscriptionRequest.h>
#include <aws/transcribestreaming/model/StartStreamTranscriptionHandler.h>
#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
#include <aws/core/utils/threading/Semaphore.h>
#include <aws/core/utils/threading/Executor.h>
#include <aws/core/utils/event/EventStream.h>

#include <fstream>
#include <cstdio>
#include <thread>

using namespace Aws;
using namespace Aws::TranscribeStreamingService;
using namespace Aws::TranscribeStreamingService::Model;

void pipe(Aws::IOStream& from, Aws::IOStream& to)
{
    char buf[1024];
    while(from)
    {
        from.read(buf, sizeof(buf));
        if(!to.write(buf, from.gcount()))
            return;
    }
}

int StartRecording(Aws::IOStream& targetStream);

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("needs a file argument.\n");
        return 1;
    }

    Aws::SDKOptions options;
    options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;
    Aws::InitAPI(options);
    {

        Client::ClientConfiguration config;
        config.executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>("", 1),
        config.region = Aws::Region::US_WEST_2;
        config.requestTimeoutMs = 1000;
        config.disableExpectHeader = true;
        // config.verifySSL = false;
        // config.endpointOverride = "transcribe-streaming-beta.us-west-2.amazonaws.com";
        // config.scheme = Aws::Http::Scheme::HTTP;
        // config.endpointOverride = "172.22.86.246:8080";
        TranscribeStreamingServiceClient client(config);

        Aws::Utils::Threading::Semaphore sem(0, 1);
        StartStreamTranscriptionHandler handler;
        handler.SetTranscriptEventCallback([](const TranscriptEvent& ev) {
                    for (auto && r : ev.GetTranscript().GetResults()) {
                    if ( r.GetIsPartial() ) printf("[partial] ");
                    else printf("[Final] ");
                        for (auto && alt : r.GetAlternatives()) {
                            printf("%s\n", alt.GetTranscript().c_str());
                        }
                    }
                });

        handler.SetOnErrorCallback([](const Aws::Client::AWSError<TranscribeStreamingServiceErrors>& ) {
                printf("got an error\n");
                });
        handler.SetBadRequestExceptionCallback([](const BadRequestException& ) {
                printf("got bad request exception\n");
                });
        handler.SetLimitExceededExceptionCallback([](const LimitExceededException& ) {
                printf("got limit exceeded exception\n");
                });
        handler.SetInternalFailureExceptionCallback([](const InternalFailureException& ) {
                printf("got internal failure exception\n");
                });
        handler.SetConflictExceptionCallback([](const ConflictException& ) {
                printf("got conflict  exception\n");
                });

        StartStreamTranscriptionRequest request;
        request.SetContentType("application/x-amz-json-1.1"); // this is a bug in the service. It should be application/vnd.amazon.eventstream
        request.SetMediaSampleRateHertz(48000);
        request.SetLanguageCode(LanguageCode::en_US);
        request.SetMediaEncoding(MediaEncoding::pcm);
        request.SetEventStreamHandler(handler);

        auto OnCallback = [&sem](const TranscribeStreamingServiceClient*,
                const Model::StartStreamTranscriptionRequest&,
                const Model::StartStreamTranscriptionOutcome&,
                const std::shared_ptr<const Aws::Client::AsyncCallerContext>&)
        {
            printf("All done!\n");
            sem.ReleaseAll();

        };

        auto requestStream = Aws::MakeShared<Aws::Utils::Event::EventEncoderStream>("");
        request.SetBody(requestStream);
        client.StartStreamTranscriptionAsync(request, OnCallback, nullptr/*context*/);
        printf("Waiting for stream to be ready..\n");
        while (!requestStream->is_ready_for_streaming()) {
            std::this_thread::yield();
        }

        if (strcmp(argv[1], "-") == 0)
        {
            StartRecording(*requestStream);
            printf("Done recording\n");
        }
        else
        {
            auto file = Aws::MakeShared<Aws::FStream>("", argv[1], std::ios_base::in | std::ios_base::binary);
            if (!file || !*file) {
                printf("unable to open file.\n");
                return 1;
            }
            printf("Writing the audio data to the stream...\n");
            pipe(*file, *requestStream); // read from GStreamer and write to the stream
        }

        /*
         * Signal to Transcribe that you're done streaming data.
         * Note: this does not "flush" the data written all the way to the service. Instead, this flushes the data in
         * the write buffer to the read buffer (wrapping and signing it in the process).
         */
        requestStream->flush();

        /*
         * Signals to the http client that you don't intend to send more data and that the request is now complete.
         * This will effectively send any data left in the read buffer to the service and end the request.
         * To send more data to the service you must create a new request and a new stream.
         */
        requestStream->close();
        sem.WaitOne();
    }

    Aws::ShutdownAPI(options);

    return 0;
}
