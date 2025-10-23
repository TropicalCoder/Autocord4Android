#include "AudioEngine.h"
// #include "SoundRecordingUtilities.h"
#include <aaudio/AAudio.h>
#include <android/log.h>
#include "LogFileHdr.h"
#include "LogFile.h"
#include <pthread.h>
#include <thread>
#include <mutex>
#include <jni.h>
#include <unistd.h>
#include <__locale>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// #define DEBUG_LOGGING
static JavaVM *javaVM;
static jclass jniMainClass;
static JNIEnv* env;
static jmethodID javaDataReadyMethodID;
// static jmethodID javaEncoderBytesExpectedMethodID;
// static jmethodID javaGetEncodeBufferMethodID;
// static jmethodID javaEncodeBufferMethodID;

/*
// Main working thread function. From a pthread,
void*  UpdateJavaThread(void* obj)
{
    return obj;
}

int AudioEngine::startThread()
{
    pthread_t       threadInfo_;
    pthread_attr_t  threadAttr_;

    pthread_attr_init(&threadAttr_);
    pthread_attr_setdetachstate(&threadAttr_, PTHREAD_CREATE_DETACHED);


    int result  = pthread_create( &threadInfo_, &threadAttr_, UpdateJavaThread, this);
    assert(result == 0);

    pthread_attr_destroy(&threadAttr_);

    (void)result;

    return 0;
}
*/

// Returns false on no more data
// (or failure to instantiate EncodeManager)
// to signal error (neither of these things should happen)
bool AudioEngine::PrepareEncodeBuffer()
{
 if(pEncodeManager == nullptr)
 {
    return false;
 }

 if(pEncodeManager->needMoreData())
 {
    ProcessData(false);
 }

 bool bRetval = pEncodeManager->FillEncodeBuffer();

 int dataRead = pEncodeManager->GetDataRead();
 if ((dataRead % 4096) == 0)
 {
     signalDataReady(dataRead);
 }

 if(!bRetval)
 {
    if(pEncodeManager != nullptr)
    {
        delete pEncodeManager;
        pEncodeManager = nullptr;
    }
 }

 return true;
}

aaudio_data_callback_result_t recordingDataCallback
        (AAudioStream __unused *stream,
         void *userData,
         void *audioData,
         int32_t numFrames)
{
    return ((AudioEngine *) userData)->recordingCallback(static_cast<float *>(audioData), numFrames);
}

aaudio_data_callback_result_t playbackDataCallback
        (AAudioStream __unused *stream,
         void *userData,
         void *audioData,
         int32_t numFrames)
{
    return ((AudioEngine *) userData)->playbackCallback(static_cast<float *>(audioData), numFrames);
}

void errorCallback(AAudioStream __unused *stream,
                   void *userData,
                   aaudio_result_t error)
{
#ifdef DEBUG_LOGGING
    __android_log_print(ANDROID_LOG_DEBUG, __func__, "error: %d", error);
#endif
    if (error == AAUDIO_ERROR_DISCONNECTED)
    {
        // The error callback expects to return immediately so it's not safe to restart our streams
        // in here. Instead we use a separate thread.
        std::function<void(void)> restartFunction = std::bind(&AudioEngine::restart,
                                                              static_cast<AudioEngine *>(userData));
        new std::thread(restartFunction);
    }
}

// Here we declare a new type: StreamBuilder which is a smart pointer to an AAudioStreamBuilder
// with a custom deleter. The function AudioStreamBuilder_delete will be called when the
// object is deleted. Using a smart pointer allows us to avoid memory management of an
// AAudioStreamBuilder.
using StreamBuilder = std::unique_ptr<AAudioStreamBuilder, decltype(&AAudioStreamBuilder_delete)>;

// Now we define a method to construct our StreamBuilder
StreamBuilder makeStreamBuilder()
{
    AAudioStreamBuilder *builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK)
    {
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_ERROR, __func__, "Failed to create stream builder %s (%d)",
                            AAudio_convertResultToText(result), result);
#endif
        return StreamBuilder(nullptr, &AAudioStreamBuilder_delete);
    }
    return StreamBuilder(builder, &AAudioStreamBuilder_delete);
}


void AudioEngine::SetMode(int nMode)
{
    Chebyshev *pChebyshev = nullptr;
    if(CheckExpired())return;
    curMode = (Mode)nMode;
    switch (curMode)
    {
        case MONITOR:
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Set mode: MONITOR");
#endif
            break;
        case RECORD:
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Set mode: RECORD");
#endif
            break;

        case INITIALIZE:
/*
 * This was the cause of a very difficult to find bug.
 * If you clicked start immediately upon boot the app would crash,
 * but if you didn't rush to start, or did something else like play,
 * it would not.
 * Seems may be some question about when the class is instatiated,
 * I'm guessing bInitialized was not initailized yet when it crashed
 * or something like that. Did not investigate further - just abandoned
 * the idea of this test...
            if(bInitialized)
            {
                break;
            }
*/
            countSamplesWrittenToDisk = 0;
            bBeganFileIO = false;
            bStopped = true;
            fpAudioData = nullptr;
            mPlaybackStream = nullptr;
            mRecordingStream = nullptr;
            if(pFFTManager == nullptr)
            {
                pFFTManager = new FFTManager(SAMPLE_RATE, kFFTSize);
            }
            if(pMusicalGraph == nullptr)
            {
                pMusicalGraph = new MusicalSpectrum();
            }

#ifdef DEBUG_LOGGING
     __android_log_print(ANDROID_LOG_DEBUG, __func__,  "Attaching filter, poles: %d, fc: %f", DEFAULT_NUM_POLES, DEFAULT_CUTOFF_FREQUENCY);
#endif
            pChebyshev = new Chebyshev(DEFAULT_CUTOFF_FREQUENCY / 32000.f, DEFAULT_PERCENT_RIPPLE, DEFAULT_NUM_POLES, HIPASS);
            fileBuffer.AttachFilter(pChebyshev, DEFAULT_NUM_POLES);
            delete pChebyshev;
            pChebyshev = nullptr;

            // Needed for testing on emulator
            oscillator.setSampleRate(32000);
            oscillator.setWaveOn(true);
            bInitialized = true;
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Set mode: INITIALIZE");
#endif
            break;
        case EXPORT:
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Set mode: EXPORT");
#endif
            fileBuffer.clear();
            break;
        case PLAY:
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Set mode: PLAY");
#endif
            fileBuffer.clear();
            break;
        case SHUTDOWN:
            if(!bStopped)stop();
            if(pFFTManager != nullptr)
            {
                delete pFFTManager;
                pFFTManager = nullptr;
            }
            if(pMusicalGraph != nullptr)
            {
                delete pMusicalGraph;
                pMusicalGraph = nullptr;
            }
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Set mode: SHUTDOWN");
#endif
            break;
        default:
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Set mode: UNKNOWN");
#endif
            break;
    }
}

void AudioEngine::InitVarsOnStartRecord()
{
    nBuffersInPreroll = 0;
    countNVTs = 0;
    countLastRecordBuffers = 0;
    countSamplesWrittenToDisk = 0;
    countEventsWrittenToLog = 0;
    totalEventsWrittenToLog = 0;
    nStartRecordingOffset = 0;
    countContiguousSamplesGroups = 0;
    countSamplesWrittenToDisk = 0;
    countLastRecordBuffers = 0;
    countEventsWrittenToLog = 0;
    totalEventsWrittenToLog = 0;
    countContiguousSamplesGroups = 0;
    countCallbacks = 0;
    bTriggerUpdatePending = false;
    bBeganFileIO = false;
    bRecording = false;
    bPaused = false;
}

void AudioEngine::start(float fPeakAmplitude)
{
 if(CheckExpired())return;
 bStopped = false;
 bAttached = false;
 memset(MusicalNote, 0, MUSICNOTEBUFSIZE);
 if((curMode == RECORD) || (curMode == MONITOR))
 {
     fileBuffer.SetScaling(0);
     oscillator.reset();
/*
 * Originally tried it this way - with filter cutoff freq set according to fTriggerFreqMin
 * Then if one wanted it set to something different than fTriggerFreqMin,
 * simply select fTriggerFreqMin to desired cutoff freq, start record, then change
 * fTriggerFreqMin to desired min frequency trigger values.
 * This works great because filter being set here on start record means that changes
 * made to fTriggerFreqMin after starting record have no effect on filter.
 * However, there isn't a whole lot of use cases for ability to set cutoff frequency.
 * Mostly I was considering the following scenario... Suppose you are capturing bird calls
 * between 3000 Hz and 4000 Hz. It might make sense to filter out all below 3000 Hz because
 * it can only be undersired noise. However, the initial recording then becomes amplified
 * on Play, and the frequencies below 3000 Hz will now appear - about 45 dB below the peaks.
 * Really, filtering for such a purpose is ideally done on post processing for this reason,
 * not on the input. The bird sounds captured in the above scenario had an "electronic edge"
 * to the sound, so really the filter cutoff needs to be much lower. Besides that, since the bird calls
 * were at or below the noise level, the recording was still noisy, only the noise was
 * high frequency. This was not such a great advantage because it ended up causing listener fatigue.
 * Furthermore, it is very unlikely that most users would understand how and when to set the filter
 * cutoff frequency.
     if(pChebyshev != nullptr)
     {
         delete pChebyshev;
         pChebyshev = nullptr;
     }

     float fc = fTriggerFreqMin;
     if(fc < 90.0)
     {
        fc = 90.0;
     }
#ifdef DEBUG_LOGGING
     __android_log_print(ANDROID_LOG_DEBUG, __func__,  "Attaching filter, poles: %d, fc: %f", DEFAULT_NUM_POLES, fc);
#endif
     pChebyshev = new Chebyshev(fc / 32000.f, DEFAULT_PERCENT_RIPPLE, DEFAULT_NUM_POLES, HIPASS);
     fileBuffer.AttachFilter(pChebyshev, DEFAULT_NUM_POLES);
*/
     fileBuffer.clear();
     if(curMode == RECORD)
     {
         InitVarsOnStartRecord();
     }
     countCallbacks = 0;
     startRecord((int)fPeakAmplitude);
 }else if((curMode == PLAY) || (curMode == EXPORT))
 {
     // We are going to use countSamplesWrittenToDisk to show countSamplesReadFromDisk
     fGlobalPeakAmplitude = fPeakAmplitude; // Saved in case of later problem fetching group peak from file
     // fileBuffer.SetScaling(fPeakAmplitude); // this caused a nasty bug on restarting Play!!!
     if(curMode == PLAY)
     {
         startPlay();
     }
 }
}

float AudioEngine::stop()
{
    bStopped = true;
    bRecording = false;
#ifdef DEBUG_LOGGING
    __android_log_print(ANDROID_LOG_DEBUG, __func__, "Stopping engine and cleanup");
#endif
    if(mRecordingStream != nullptr)
    {
       // If Stop() was called while we were still recording...
       if(bBeganFileIO)
       {
           fileBuffer.clear();
           bBeganFileIO = false;
           if(bAccessLog)
           {
               totalEventsWrittenToLog += countEventsWrittenToLog;
               int samplesInGroup = countSamplesWrittenToDisk - nStartRecordingOffset;
               int callbacksInGroup = samplesInGroup / 4096;
               logFile.writeEndOfGroup(countEventsWrittenToLog, callbacksInGroup);
#ifdef DEBUG_LOGGING
               __android_log_print(ANDROID_LOG_DEBUG, __func__,
                                   "Stopped recording - callbacksInGroup in group: %d, countEventsWrittenToLog: %d",
                                   callbacksInGroup, countEventsWrittenToLog);
#endif
           }
           countEventsWrittenToLog = 0;
           // Contiguous events have ended. Get the peak amplitude of all contiguous samples in the event group
           float peakAmps = fileBuffer.GetPeakAmplitude();
           if(fpEventGroupPeaks != nullptr)
           {
               fwrite(&peakAmps, sizeof(float), 1, fpEventGroupPeaks);
           }
#ifdef DEBUG_LOGGING
           __android_log_print(ANDROID_LOG_DEBUG, __func__, "Peak amplitude in group: %1.3f, dB: %1.1f", peakAmps, 20.f * log10(peakAmps));
#endif
           fileBuffer.InitPeakAmplitude();
       }
       logFile.writeEndOfFile(totalEventsWrittenToLog, countContiguousSamplesGroups);

#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "countContiguousSamplesGroups: %d", countContiguousSamplesGroups);
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "totalEventsWrittenToLog: %d", totalEventsWrittenToLog);
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "countSamplesWrittenToDisk: %d", countSamplesWrittenToDisk);
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "countCallbacks: %d", countCallbacks);
#endif
       stopStream(mRecordingStream);
       closeStream(&mRecordingStream);
        mRecordingStream = nullptr;
    }

    if(mPlaybackStream != nullptr)
    {
       stopStream(mPlaybackStream);
       closeStream(&mPlaybackStream);
        mPlaybackStream = nullptr;
    }
    closeFile();
    return fileBuffer.GetScaling();
}

void AudioEngine::startPlay()
{
    // Create the playback stream.
    StreamBuilder playbackBuilder = makeStreamBuilder();
    AAudioStreamBuilder_setSampleRate(playbackBuilder.get(), SAMPLE_RATE);
    AAudioStreamBuilder_setFormat(playbackBuilder.get(), AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(playbackBuilder.get(), kChannelCountMono);
    // AAudioStreamBuilder_setPerformanceMode(playbackBuilder.get(), AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    // AAudioStreamBuilder_setSharingMode(playbackBuilder.get(), AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setDataCallback(playbackBuilder.get(), ::playbackDataCallback, this);
    AAudioStreamBuilder_setErrorCallback(playbackBuilder.get(), ::errorCallback, this);
    AAudioStreamBuilder_setFramesPerDataCallback(playbackBuilder.get(), kFramesPerDataCallback);

    aaudio_result_t result = AAudioStreamBuilder_openStream(playbackBuilder.get(), &mPlaybackStream);

    if (result != AAUDIO_OK)
    {
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__,
                            "Error opening playback stream %s",
                            AAudio_convertResultToText(result));
#endif
        return;
    }

#ifdef DEBUG_LOGGING
    int32_t sampleRate = AAudioStream_getSampleRate(mPlaybackStream);
    __android_log_print(ANDROID_LOG_DEBUG, __func__, "sampleRate: %d", sampleRate);
#endif
    result = AAudioStream_requestStart(mPlaybackStream);
    if (result != AAUDIO_OK)
    {
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__,
                            "Error starting playback stream %s",
                            AAudio_convertResultToText(result));
#endif
        closeStream(&mPlaybackStream);
        return;
    }

}

void AudioEngine::startRecord(int deviceID)
{
    // Create the recording stream.
    StreamBuilder recordingBuilder = makeStreamBuilder();
    if(deviceID >= 0)
    {
        AAudioStreamBuilder_setDeviceId(recordingBuilder.get(), deviceID);
    }
    AAudioStreamBuilder_setDirection(recordingBuilder.get(), AAUDIO_DIRECTION_INPUT);
    // AAudioStreamBuilder_setPerformanceMode(recordingBuilder.get(),  AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    // AAudioStreamBuilder_setSharingMode(recordingBuilder.get(), AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setFormat(recordingBuilder.get(), AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setSampleRate(recordingBuilder.get(), SAMPLE_RATE);
    AAudioStreamBuilder_setChannelCount(recordingBuilder.get(), kChannelCountMono);  // *Note mono!
    AAudioStreamBuilder_setDataCallback(recordingBuilder.get(), ::recordingDataCallback, this);
    AAudioStreamBuilder_setErrorCallback(recordingBuilder.get(), ::errorCallback, this);
    // AAudioStreamBuilder_setSamplesPerFrame(recordingBuilder.get(), 64); // ??? Identical to AAudioStreamBuilder_setChannelCount().
    // AAudioStreamBuilder_setBufferCapacityInFrames(recordingBuilder.get(), 4096);
    AAudioStreamBuilder_setFramesPerDataCallback(recordingBuilder.get(), kFramesPerDataCallback);
    aaudio_result_t result = AAudioStreamBuilder_openStream(recordingBuilder.get(), &mRecordingStream);


    if (result != AAUDIO_OK)
    {
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__,
                            "Error opening recording stream %s",
                            AAudio_convertResultToText(result));
#endif
        closeStream(&mRecordingStream);
        return;
    }

    result = AAudioStream_requestStart(mRecordingStream);
    if (result != AAUDIO_OK)
    {
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__,
                            "Error starting recording stream %s",
                            AAudio_convertResultToText(result));
#endif
        return;
    }
}

// Called from errorCallback()
void AudioEngine::restart()
{
#ifdef DEBUG_LOGGING
    __android_log_print(ANDROID_LOG_DEBUG, __func__, "Called from errorCallback()");
#endif
    bAttached = false;
    countCallbacks = 0;
    static std::mutex restartingLock;
    if (restartingLock.try_lock())
    {
        // stop();
        if(mRecordingStream != nullptr)
        {
            stopStream(mRecordingStream);
            closeStream(&mRecordingStream);
            mRecordingStream = nullptr;
        }
        if(mPlaybackStream != nullptr)
        {
            stopStream(mPlaybackStream);
            closeStream(&mPlaybackStream);
            mPlaybackStream = nullptr;
        }
        if((curMode == RECORD) || (curMode == MONITOR))
        {
            fileBuffer.clear();
            startRecord(-1);
        }else if(curMode == PLAY)
        {
            startPlay();
        }
        restartingLock.unlock();
    }
}

// Expects we are in EXPORT mode
void AudioEngine::OpenExportTextFile(char *pPathname, int fileSizeBytes)
{
    if((curMode != EXPORT))
    {
        return;
    }

    char txtpathname[256];
    memset(txtpathname, 0, 256);
    strcpy(txtpathname, MediaPathname);
    char *ptrExt = strrchr(txtpathname, (int) '.');
    ++ptrExt;
    *ptrExt++ = 't';
    *ptrExt++ = 'x';
    *ptrExt++ = 't';
    *ptrExt++ = '\0'; // Extension could be "flac"

#ifdef DEBUG_LOGGING
    __android_log_print(ANDROID_LOG_DEBUG, __func__, "Text path: %s", txtpathname);
#endif

    FILE *fpExportText = fopen(txtpathname, "wt");
    if(fpExportText == nullptr)return;

    memset(txtpathname, 0, 256);
    strcpy(txtpathname, pPathname);

    ptrExt = strrchr(txtpathname, (int) '.');
    ++ptrExt;
    *ptrExt++ = 'h';
    *ptrExt++ = 'd';
    *ptrExt++ = 'r';
    *ptrExt++ = '\0';

#ifdef DEBUG_LOGGING
    __android_log_print(ANDROID_LOG_DEBUG, __func__, "Header path: %s", txtpathname);
#endif

    // Copy the header info to the export text file
    FILE *fp = fopen(txtpathname, "rt");
    if(fp != nullptr)
    {
        memset(txtpathname, 0, 256);
        int lineCount = 0;
        while (!feof(fp) && (lineCount < 4))
        {
            ++lineCount;
            fgets(txtpathname, 64, fp);
            fputs(txtpathname, fpExportText);
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Export: %s", txtpathname);
#endif
            memset(txtpathname, 0, 64);
        }
        fclose(fp);
    }

    int countSamples = fileSizeBytes / 2;
    sprintf(txtpathname, "Samples recorded:  %d\n", countSamples);
#ifdef DEBUG_LOGGING   // "Export: Samples recorded:  5758976"
    __android_log_print(ANDROID_LOG_DEBUG, __func__, "Export: %s", txtpathname);
#endif
    fputs(txtpathname, fpExportText);

    double dSeconds = (double) countSamples / 32000;
    int seconds = (int) floor(dSeconds + 0.5);
    int minutes = seconds / 60;
    int hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;
    hours %= 100; // // What if >= 100 hours? We roll over to start at 0 hours again

    memset(txtpathname, 0, 256);
    sprintf(txtpathname, "Duration [hh:mm:ss] %2.2d:%2.2d:%2.2d\n", hours, minutes, seconds);
#ifdef DEBUG_LOGGING   // "Export: Duration [hh:mm:ss] 00:03:00"
    __android_log_print(ANDROID_LOG_DEBUG, __func__, "Export: %s", txtpathname);
#endif
    fputs(txtpathname, fpExportText);
    fputc('\n', fpExportText);

    logFile.ExportOnRead(fpExportText);
}


// Called before engine start if MODE is PLAY or RECORD
void AudioEngine::openFile(char *pPathname, int fileSizeBytes)
{
#ifdef DEBUG_LOGGING
    __android_log_print(ANDROID_LOG_DEBUG, __func__, "Audio data pathname: %s", pPathname);
#endif
    bBeganFileIO = false;
    if(curMode == RECORD)
    {
       fpAudioData = fopen(pPathname, "wb");

       auto *pLogFileHdr = new LogFileHdr();
       int len = (int)strlen(pPathname) - 3;
       pPathname[len++] = 'h';
       pPathname[len++] = 'd';
       pPathname[len] = 'r';
#ifdef DEBUG_LOGGING
       __android_log_print(ANDROID_LOG_DEBUG, __func__, "Log hdr pathname: %s", pPathname);
#endif

       if(pLogFileHdr->OpenForWrite(pPathname))
       {
          pLogFileHdr->writeFileHeader(fTriggerFreqMin, fTriggerFreqMax, fTriggerDBs);
          int fileLen = pLogFileHdr->CloseFile();
#ifdef DEBUG_LOGGING
          __android_log_print(ANDROID_LOG_DEBUG, __func__, "Wrote Log hdr - fileLen: %d", fileLen);
#endif
       }
       delete pLogFileHdr;

       len = (int)strlen(pPathname) - 3;
       pPathname[len++] = 'l';
       pPathname[len++] = 'o';
       pPathname[len] = 'g';
       bAccessLog = logFile.OpenForWrite(pPathname);

       if(bAccessLog)
       {
#ifdef DEBUG_LOGGING
          __android_log_print(ANDROID_LOG_DEBUG, __func__, "Log data pathname: %s", pPathname);
#endif
          logFile.writeVersion(CURRENT_LOG_VERSION);
       }

       len = (int)strlen(pPathname) - 3;
       pPathname[len++] = 'p';
       pPathname[len++] = 'k';
       pPathname[len] = 's';

       fpEventGroupPeaks = fopen(pPathname, "wb");
#ifdef DEBUG_LOGGING
       if(fpEventGroupPeaks != nullptr)
       {
          __android_log_print(ANDROID_LOG_DEBUG, __func__, "Peaks file pathname: %s", pPathname);
       }
#endif
     }else if((curMode == PLAY) || (curMode == EXPORT))
     {
         fpAudioData = fopen(pPathname, "rb");

         if (fpAudioData != nullptr)
         {
#ifdef DEBUG_LOGGING
             __android_log_print(ANDROID_LOG_DEBUG, __func__, "Open audio data file success!");
#endif

             int len = (int)strlen(pPathname) - 3;
             pPathname[len++] = 'p';
             pPathname[len++] = 'k';
             pPathname[len] = 's';
             fpEventGroupPeaks = fopen(pPathname, "rb");
             if(fpEventGroupPeaks != nullptr)
             {
                 FetchPeak(false); // Passing true whenever rewound
#ifdef DEBUG_LOGGING
                 __android_log_print(ANDROID_LOG_DEBUG, __func__, "Open peaks file success!");
#endif
             }

             len = (int)strlen(pPathname) - 3;
             pPathname[len++] = 'l';
             pPathname[len++] = 'o';
             pPathname[len] = 'g';
             bAccessLog = logFile.OpenForRead(pPathname);
             if(bAccessLog)
             {
                 nBuffersInPreroll = logFile.readVersion();
                 if(nBuffersInPreroll > -1)
                 {
#ifdef DEBUG_LOGGING
                     __android_log_print(ANDROID_LOG_DEBUG, __func__,
                                         "Open log file success - vers: %d", nBuffersInPreroll);
#endif

                     OpenExportTextFile(pPathname, fileSizeBytes);

                     nBuffersInPreroll = logFile.readBuffersInPreroll();
                     if(nBuffersInPreroll == -1)
                     {
                         bAccessLog = false;
                     }
                     nBuffersInPreroll = 0; // Immediately fetch first event
                 }else bAccessLog = false; // Empty file!
             }
             currentAudioFileSampleLength = fileSizeBytes / 2;
             currentAudioFileRemainingSamples = currentAudioFileSampleLength;
             if(curMode == EXPORT)
             {
#ifdef DEBUG_LOGGING
                 __android_log_print(ANDROID_LOG_DEBUG, __func__, "nEncodeBufSize: %d",  nEncodeBufSize);
#endif
                pEncodeManager = new EncodeManagerPCM(pEncodeBuffer, fileBuffer.GetDataPointer(), nEncodeBufSize / 2,  currentAudioFileSampleLength);
                // pEncodeManager = new EncodeManagerFloat(pEncodeBuffer, fileBuffer.GetDataPointer(), nEncodeBufSize / 4,  currentAudioFileSampleLength);
             }
             fileBuffer.clear();
             bStopped = false; // gotta do this now or ProcessData() will do nothing - just return
             ProcessData(true);
             bStopped = true; // Seems if I leave this false it crashes!
#ifdef DEBUG_LOGGING
             __android_log_print(ANDROID_LOG_DEBUG, __func__, "currentAudioFileRemainingSamples: %d",
                                 currentAudioFileRemainingSamples);
         } else
         {
             __android_log_print(ANDROID_LOG_DEBUG, __func__, "Failed fopen for PLAY");
#endif
         }
     }
}

void AudioEngine::closeFile()
{
    if(fpEventGroupPeaks != nullptr)
    {
        int bytes = (int)ftell(fpEventGroupPeaks);
        fclose(fpEventGroupPeaks);
        fpEventGroupPeaks = nullptr;
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "Closed peaks file - num peaks stored: %d", (int)(bytes / sizeof(float)));
#endif
    }

    if(bAccessLog)
    {
        int fileLength = logFile.CloseFile();
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "countContiguousSamplesGroups: %d", countContiguousSamplesGroups);
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "Closed Log file - length: %d", fileLength);
#endif
    }
    if(fpAudioData != nullptr)
    {
        fclose(fpAudioData);
        fpAudioData = nullptr;
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "Closed audio data file!");
    } else
    {
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "Audio data file was closed!");
#endif
    }
}

aaudio_data_callback_result_t AudioEngine::recordingCallback(float *audioData, int32_t numFrames)
{
    if(bPaused)return AAUDIO_CALLBACK_RESULT_CONTINUE;
    // oscillator.render(audioData, numFrames);
    fileBuffer.write(audioData, numFrames);
    signalDataReady(0);
    ++countCallbacks;
/*
    if((countCallbacks % 10) == 0)
    {
        oscillator.IncrementFrequency();
    }
*/
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

aaudio_data_callback_result_t AudioEngine::playbackCallback(float *audioData, int32_t numFrames)
{
    if(bPaused)
    {
       memset(audioData, 0, sizeof(float) * numFrames);
       return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
    fileBuffer.read(audioData, numFrames, nullptr);
    signalDataReady(0);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}


void AudioEngine::stopStream(AAudioStream *stream) const
{
    static std::mutex stoppingLock;
    stoppingLock.lock();
    if (stream != nullptr)
    {
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "Stopping stream");
#endif
        aaudio_result_t result = AAudioStream_requestStop(stream);
#ifdef DEBUG_LOGGING
        if (result != AAUDIO_OK)
        {
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Error stopping stream %s",
                                AAudio_convertResultToText(result));
        }
#endif
    }
    stoppingLock.unlock();
}

void AudioEngine::closeStream(AAudioStream **stream)
{

    static std::mutex closingLock;
    closingLock.lock();
    if (*stream != nullptr)
    {
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "Closing stream");
#endif
        aaudio_result_t result = AAudioStream_close(*stream);
#ifdef DEBUG_LOGGING
        if (result != AAUDIO_OK)
        {
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Error closing stream %s",
                                AAudio_convertResultToText(result));
        }
#endif
        *stream = nullptr;
    }
    closingLock.unlock();
}

// See https://developer.android.com/training/articles/perf-jni
// https://riptutorial.com/android/example/27060/how-to-call-a-java-method-from-native-code
void AudioEngine::CacheCallbackParams(JavaVM *vm, jclass claz, JNIEnv* environment)
{
    javaVM = vm;
    jniMainClass = claz;
    env = environment;
#ifdef DEBUG_LOGGING
    __android_log_print(ANDROID_LOG_ERROR, __func__, "Attaching to Current Thread");
#endif
    int res = javaVM->AttachCurrentThread(&env, nullptr);
    if (JNI_OK != res)
    {
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_ERROR, __func__, "Failed to AttachCurrentThread, ErrorCode = %d", res);
#endif
        return;
    }
    javaDataReadyMethodID = env->GetStaticMethodID(jniMainClass, "dataReady", "(I)V");
    // javaEncoderBytesExpectedMethodID = env->GetStaticMethodID(jniMainClass, "encoderBytesExpected", "()I"); // public static int encoderBytesExpected()
    // javaEncodeBufferMethodID = env->GetStaticMethodID(jniMainClass, "encodeBuffer", "()Z"); // public static boolean encodeBuffer()
    // javaEncodeBufferMethodID = env->GetStaticMethodID(jniMainClass, "encodeBuffer", "([B)Z"); // public static boolean encodeBuffer(ByteBuffer rawData)
    // __android_log_print(ANDROID_LOG_ERROR, __func__, "javaDataReadyMethodID: %d", javaDataReadyMethodID);
}


/*
 * Left here as examples...
int AudioEngine::getEncoderExpectedBytes()
{
    if(!bAttached)
    {
       ...
       bAttached = true;
    }
    if(javaEncoderBytesExpectedMethodID != nullptr)
    {
        return env->CallStaticIntMethod(jniMainClass, javaEncoderBytesExpectedMethodID);
    }
    return 0;
}

bool AudioEngine::encodeBuffer()
{
    bool bRetVal = false;

    if(!bAttached)
    {
        ...
        bAttached = true;
    }

    if(javaEncodeBufferMethodID != nullptr)
    {
        bRetVal = env->CallStaticBooleanMethod(jniMainClass, javaEncodeBufferMethodID);
    }
    return bRetVal;
}

bool AudioEngine::encodeBuffer(__uint8_t* byteArray )
{
 bool bRetVal = false;

    if(!bAttached)
    {
        ...
        bAttached = true;
    }

 if(javaEncodeBufferMethodID != nullptr)
 {
     int data_size = nEncodeBufSize;
     jbyteArray retArray = env->NewByteArray(data_size);
     if(env->GetArrayLength(retArray) != data_size)
     {
         env->DeleteLocalRef(retArray);
         retArray = env->NewByteArray(data_size);
     }
     void *temp = env->GetPrimitiveArrayCritical((jarray)retArray, nullptr);
     memcpy(temp, byteArray, static_cast<size_t>(data_size));
     env->ReleasePrimitiveArrayCritical(retArray, temp, 0);

     bRetVal = env->CallStaticBooleanMethod(jniMainClass, javaEncodeBufferMethodID, retArray);
 }
 return bRetVal;
}
*/

void AudioEngine::signalDataReady(int dataExported)
{
    if(!bAttached)
    {
        int res = javaVM->AttachCurrentThread(&env, nullptr);
        if (JNI_OK != res)
        {
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_ERROR, __func__,
                                "Failed to AttachCurrentThread, ErrorCode = %d", res);
#endif
            return;
        }
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_ERROR, __func__,
                            "Success AttachCurrentThread, ErrorCode = %d", res);
#endif
        bAttached = true;
    }
    env->CallStaticVoidMethod(jniMainClass, javaDataReadyMethodID, (jint)dataExported);
}

float AudioEngine::GetMusicalNote(int x)
{
    if(pMusicalGraph == nullptr)return 0;
    return pMusicalGraph->GetMusicalNote(x);
}

int AudioEngine::GetFrequencyMusicalNoteArrayOffset(float f)
{
    if(pMusicalGraph == nullptr)return 0;
    return pMusicalGraph->FrequencyToMusicalNoteArrayOffset(f);
}

int AudioEngine::GetSamplesPlayed()
{
    return currentAudioFileSampleLength - currentAudioFileRemainingSamples;
}

char *AudioEngine::GetTimeRecorded()
{
    return &TimeBuf[0];
}

void AudioEngine::ShowPeak(float fPeakFrequency, float dB)
{
    if(dB > -pFFTManager->GetMaxDB())
    {
        memset(&mTextBuf[0], 0, TEXTBUFSIZE);

        if(fPeakFrequency < 10000.f)
        {
            sprintf(&mTextBuf[0], "Peak frequency: %5.2f, dB: %2.1f", fPeakFrequency, dB);
        }else
        {
            sprintf(&mTextBuf[0], "Peak frequency: %5.1f, dB: %2.1f", fPeakFrequency, dB);
        }
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "%s", &mTextBuf[0]);
    } else
    {
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "Silent buffer!");
#endif
    }
}

void AudioEngine::FetchPeak(bool bRewind)
{
    if(fpEventGroupPeaks != nullptr)
    {
        if(bRewind)
        {
            fseek(fpEventGroupPeaks, 0, SEEK_SET); // Rewind the file
        }

        fLocalPeakAmplitude = 0;
        int reval = 0;
        if(!feof(fpEventGroupPeaks))
        {
            reval = fread(&fLocalPeakAmplitude, sizeof(float), 1, fpEventGroupPeaks);
        }
        if((reval == 1) && (fLocalPeakAmplitude > 0.00001))
        {
            fileBuffer.SetScaling(fLocalPeakAmplitude);
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Applied gain for group: %1.1f dB, PeakAmplitude: %f", -20.f * log10(fLocalPeakAmplitude), fLocalPeakAmplitude);
#endif
        } else
        {
            fLocalPeakAmplitude = fGlobalPeakAmplitude; // Was saved in case of later problem fetching group peak from file
            fileBuffer.SetScaling(fGlobalPeakAmplitude);
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Failed fetching peak amplitude from file - reval %d, fPeak: %f", reval, fLocalPeakAmplitude);
#endif
        }
    } // else if no file or can't access file,
      // peak amplitude was set in start() and will remain as long as we are playing
}

bool AudioEngine::PlayCallbackFetchesAudioData(int nSamplesToFetch)
{
    if(fpAudioData != nullptr)
    {
        // EOF?
        if (currentAudioFileRemainingSamples < nSamplesToFetch)
        {
            // Rewind the files
            if (bAccessLog)
            {
                logFile.Rewind();
                if (logFile.readVersion() > 0)
                {
                    nBuffersInPreroll = logFile.readBuffersInPreroll() - 1;
                    if (nBuffersInPreroll == -1)bAccessLog = false;
                    nBuffersInPreroll = 0;
                    countSamplesWrittenToDisk = 0; // to show samples read from disk
                } else bAccessLog = false;
            }

            fileBuffer.clear();
            FetchPeak(true); // Passing true whenever rewound

            fseek(fpAudioData, 0, SEEK_SET);
            currentAudioFileRemainingSamples = currentAudioFileSampleLength;
            countCallbacks = 0;

            // Begin emulation of call to ProcessData() as done when we first load the file
            memset(&mCurrentSampleData[0], 0, (size_t) kMaxSamples);
            fileBuffer.read(&mPreviousSampleData[0], kFFTSize, nullptr);
            if (bAccessLog)
            {
               FileItems fileItem;
               int val1 = 0, val2 = 0;
               char tmpBuf[TIMEBUFSIZE];
               memset(tmpBuf, 0, TIMEBUFSIZE);
               fileItem = logFile.ReadNextItem(tmpBuf, &mTextBuf[0], &val1, &val2);
               if(fileItem == EVT) // Better be!
               {
                   countSamplesWrittenToDisk = -val1; // Pass negative to indicate start of group
#ifdef DEBUG_LOGGING
                   __android_log_print(ANDROID_LOG_DEBUG, __func__, "Began new group on rewind!");
#endif
                   strcpy(TimeBuf, tmpBuf);
                   countNVTs = 0;
                   // nBuffersInPreroll = 2;
#ifdef DEBUG_LOGGING
                   __android_log_print(ANDROID_LOG_DEBUG, __func__, "Event time: %s", TimeBuf);
                   __android_log_print(ANDROID_LOG_DEBUG, __func__,
                                       "Event offset: %d, current offset: %d", val1,
                                       GetSamplesPlayed());
#endif
                   nSamplesToFetch = kFFTSize;
                   bBeganFileIO = true;
               }
            }
            bRewound = true;
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Rewound the file!");
#endif
        }

        // This is what normally happens...
        // Read the required num samples in from disk (were stored as int16s)
        // and call fileBuffer to convert them to floating point

        // Maintain FFT size samples prebuffered at all times
        int16_t *pBuf = fileBuffer.GetFileDataArray();
        currentAudioFileRemainingSamples -= fread(pBuf, sizeof(int16_t), (size_t) nSamplesToFetch, fpAudioData);
        fileBuffer.write(nullptr, nSamplesToFetch);
    }
    return bRewound;
}


bool AudioEngine::HandlePlay(bool bCalledFromCode)
{
    // Fetch old data for processing by the FFT
    memcpy(&mCurrentSampleData[0], &mPreviousSampleData[0], sizeof(float) * kFFTSize);
    fileBuffer.read(&mPreviousSampleData[0], kFFTSize, nullptr);


    // First time we fill the buffer, and thereafter only kFramesPerDataCallback samples
    int samplesToFetch;
    if (!bBeganFileIO)
    {
        samplesToFetch = kFFTSize;
        nBuffersInPreroll -= 1; // Because we will fetch 2 buffers of audio at the start
        bBeganFileIO = true;
    } else
    {
        samplesToFetch = kFramesPerDataCallback;
    }


    if(bAccessLog)
    {
        // Count down to first event group
        if(nBuffersInPreroll > 1)
        {
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Skipped preroll buffer");
#endif
            --nBuffersInPreroll;
        } else
        {
            bool bStartGroup = false;
            if(nBuffersInPreroll)
            {
                bStartGroup = true;
                nBuffersInPreroll = 0;
                // Gain was applied on call to this func from openFile
                // or upon rewind of the files - don't do it again!
                // if(!bGainApplied)
                // {
                //    FetchPeak(false);
                // }
                // Was this, but we need the gain in time for the preroll
            }
            FileItems fileItem;
            int val1 = 0, val2 = 0;
            char tmpBuf[TIMEBUFSIZE];
            memset(tmpBuf, 0, TIMEBUFSIZE);
            fileItem = logFile.ReadNextItem(tmpBuf, &mTextBuf[0], &val1, &val2);
            if (fileItem == NVT)
            {
#ifdef DEBUG_LOGGING
                __android_log_print(ANDROID_LOG_DEBUG, __func__, "NVT");
#endif
                ++countNVTs;
            } else if (fileItem == EVT)
            {
                strcpy(TimeBuf, tmpBuf);
                // Parse out the peak frequency from "Peak frequency: 25.940, dB: -55.9"
                char *ptr = strchr(&mTextBuf[0], ':');
                if(ptr != nullptr)
                {
                   ++ptr;
                   char tmp[16];
                   char *ptrTmp = &tmp[0];
                   memset(tmp, 0, 16);
                   while(*ptr != ',')
                   {
                       *ptrTmp++ = *ptr++;
                   }
                   // Supposedly we now have the frequency in tmp[]
                   auto freq = (float)strtod(tmp, &ptrTmp);
                   ptrTmp = pMusicalGraph->GetMusicalNoteTextVal(freq);
                   memset(MusicalNote, 0, MUSICNOTEBUFSIZE);
                   if(ptrTmp != nullptr)
                   {
                      strcpy(MusicalNote, ptrTmp);
                   }
                }

                countNVTs = 0;
                if(bStartGroup)
                {
                    countSamplesWrittenToDisk = -val1; // Pass negative to indicate start of group
#ifdef DEBUG_LOGGING
                    __android_log_print(ANDROID_LOG_DEBUG, __func__, "Began new group!");
#endif
                } else
                {
                    countSamplesWrittenToDisk = val1;
                    // Event offset should be = offset recorded in log
                    if(!bRewound && (countSamplesWrittenToDisk != GetSamplesPlayed()))
                    {
                        strcpy(TimeBuf, "--:--:--"); // For now just show we are out of sync
#ifdef DEBUG_LOGGING
                        __android_log_print(ANDROID_LOG_DEBUG, __func__, "Out of sync!");
#endif
                    }
                }
#ifdef DEBUG_LOGGING
                __android_log_print(ANDROID_LOG_DEBUG, __func__, "Event time: %s", TimeBuf);
                __android_log_print(ANDROID_LOG_DEBUG, __func__,
                                    "Event offset: %d, current offset: %d",  val1, GetSamplesPlayed());
#endif
                if(bCalledFromCode || bRewound)
                {
                    // We fetched this event immediately upon file load or rewind
                    // We now need to wait before fetching the next and subsequent events
                    nBuffersInPreroll = 2;
                    bRewound = false;
                }
            } else if (countNVTs == 4)
            {
                // Expecting an EOG
                if (fileItem != EOG)
                {
#ifdef DEBUG_LOGGING
                    __android_log_print(ANDROID_LOG_DEBUG, __func__, "Error: Expected an EOG!");
#endif
                } else
                {
                    int buffers = GetSamplesPlayed() / 4096;
#ifdef DEBUG_LOGGING
                    __android_log_print(ANDROID_LOG_DEBUG, __func__,
                                        "End of events group - countEvents: %d, callbacks: %d, buffers %d",
                                        val1, val2, buffers);
#endif
                    // Now expecting to begin next group (or FEOF)
                    fileItem = logFile.ReadNextItem(tmpBuf, &mTextBuf[0], &val1, &val2);
                    if (fileItem == BPR)
                    {
                        nBuffersInPreroll = val1;
                        FetchPeak(false); // New 20200624 - we need to apply the new gain to the preroll
#ifdef DEBUG_LOGGING
                        __android_log_print(ANDROID_LOG_DEBUG, __func__, "BPR nBuffersInPreroll: %d", nBuffersInPreroll);
                    } else if (fileItem == FEOF)
                    {
                        __android_log_print(ANDROID_LOG_DEBUG, __func__,
                                            "End of log file - Total events: %d, Count contiguous events groups: %d",
                                            val1, val2);
#endif
                    } else if (fileItem == ERR)
                    {
                        bAccessLog = false;
                        fLocalPeakAmplitude = fGlobalPeakAmplitude; // Was saved in case of later problem fetching group peak from file
                        fileBuffer.SetScaling(fGlobalPeakAmplitude);
#ifdef DEBUG_LOGGING
                        __android_log_print(ANDROID_LOG_DEBUG, __func__,
                                            "Error returned from logFile.ReadNextItem() !");
#endif
                    }
                } // End if(Expecting an EOG)
                countNVTs = 0;
            } else if (fileItem == BPR)
            {
                nBuffersInPreroll = val1;
                FetchPeak(false); // New 20200624 - we need to apply the new gain to the preroll
#ifdef DEBUG_LOGGING
                __android_log_print(ANDROID_LOG_DEBUG, __func__, "BPR nBuffersInPreroll: %d", nBuffersInPreroll);
            } else if (fileItem == FEOF)
            {
                __android_log_print(ANDROID_LOG_DEBUG, __func__,
                                    "End of log file - Total events: %d, Count contiguous events groups: %d",
                                    val1, val2);
#endif
            } else if (fileItem == ERR)
            {
                bAccessLog = false;
                fLocalPeakAmplitude = fGlobalPeakAmplitude; // Was saved in case of later problem fetching group peak from file
                fileBuffer.SetScaling(fGlobalPeakAmplitude);
#ifdef DEBUG_LOGGING
                __android_log_print(ANDROID_LOG_DEBUG, __func__,
                                    "Error returned from logFile.ReadNextItem() !");
#endif
            }
        }
    }

    PlayCallbackFetchesAudioData(samplesToFetch);
    // if(samplesToFetch == kFFTSize)return false;
    // This was the first fetch for audio data called from fileIO()
    // No point in doing the FFT because there will be no call to RenderGraph() to display it

    return samplesToFetch != kFFTSize;
}

int AudioEngine::HandleRecord(float fPeakFrequency, float dB)
{
    bRecording = false;

    int initialBuffersToWrite = 5;
    if (!bBeganFileIO)
    {
        if((countLastRecordBuffers >= 6) && (countLastRecordBuffers <= 8))
        {
            initialBuffersToWrite = countLastRecordBuffers - 4;
            ++countLastRecordBuffers;
        }
    }

    if(bTriggerUpdatePending)
    {
        if(fPendingTriggerDBs != fTriggerDBs)
        {
            fTriggerDBs = fPendingTriggerDBs;
            logFile.WriteTriggerDB(fTriggerDBs);
        }
        if(fPendingTriggerFreqMin != fTriggerFreqMin)
        {
            fTriggerFreqMin = fPendingTriggerFreqMin;
            logFile.WriteMinTriggerFrequency(fTriggerFreqMin);
        }
        if(fPendingTriggerFreqMax != fTriggerFreqMax)
        {
            fTriggerFreqMax = fPendingTriggerFreqMax;
            logFile.WriteMaxTriggerFrequency(fTriggerFreqMax);
        }
        bTriggerUpdatePending = false;
    }

    bool bTriggered = ((dB > fTriggerDBs) && (fPeakFrequency >= fTriggerFreqMin) && (fPeakFrequency <= fTriggerFreqMax));
    if(bTriggered)
    {
        countLastRecordBuffers = 0;
    } else
    {
        if(bBeganFileIO)
        {
            ++countLastRecordBuffers;
        }
    }

    // When trigger ends, we want to continue writing the tail to disk for 4 more callbacks
    if (bTriggered || (bBeganFileIO && (countLastRecordBuffers <= 4)))
    {
        if (fpAudioData != nullptr)
        {
            bRecording = true;
            memset(TimeBuf, 0, TIMEBUFSIZE);
            logFile.OutPutTimeString(TimeBuf);
#ifdef DEBUG_LOGGING
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "Recording %s", TimeBuf);
#endif
            // If !bTriggered, we are here because (bBeganFileIO && (countLastRecordBuffers <= 4) = true

            if (!bBeganFileIO)
            {
                // ShowPeak(fPeakFrequency, dB); // vers. 1.17 Why was this info only available when triggered?
                ++countContiguousSamplesGroups;
                nStartRecordingOffset = countSamplesWrittenToDisk;
                countEventsWrittenToLog = 0;
                fileBuffer.InitPeakAmplitude();

                if(bAccessLog)
                {
                    logFile.writeBuffersInPreroll(initialBuffersToWrite - 1);
                    logFile.writeEvent(&mTextBuf[0], countSamplesWrittenToDisk);
                    ++countEventsWrittenToLog;
                }
                // The very first time we trigger, we want the current buffer (of kFramesPerDataCallback samples)
                // plus the approximately half a second of data that preceded it (kFFTSize samples)
                // kFFTSize + kFramesPerDataCallback = kMaxSamples in total
                // However, as we continue recording then trigger off, when we trigger on again,
                // if that wasn't so long after the last trigger, we could end up duplicating data written to disk.
                //
                // In previous versions I cleared the file buffer after writing the last buffer of the event group.
                // Then when we next triggered, we got a buffer of silence that momentarily blanked the screen.
                // The comment was that I was doing this to "clear out the buffer in case we trigger again immediately".
                // Then I commented out the call to 'fileBuffer.clear()', realizing that was good data, contiguous with the next fetch
                // and there was no need to throw it away. However, when I looked at this code here where we begin a new events group
                // I saw that we fetched kMaxSamples to begin write to disk. If the buffer was not cleared out and we had retriggered
                // immediately, we will write - the latest new record data, data from last record callback ignored while wrapping up the events group,
                // plus data from three previous callbacks that were already on disk, duplicating 3 buffers we already wrote to disk!

                // What we need to do is only write new data to disk (dah!).
                // Now I have added the var initialBuffersToWrite above, which will tell us how many buffers we should write.
                // If we have not yet made our first disk write, initialBuffersToWrite remains 5 as initialized, because that var
                // never changes until we have recorded our first events group as determined by testing 'countLastRecordBuffers'.
                // Now imagine we just finished our first event group, (countLastRecordBuffers == 5), we set bBeganFileIO = false,
                // and close the group, ignoring the latest buffer of fresh data returned from the record callback.
                // countLastRecordBuffers is incremented, leaving it countLastRecordBuffers = 6
                // Next callback we are immediately retriggered. We engage
                //            if((countLastRecordBuffers >= 6) && (countLastRecordBuffers <= 8))
                //            {
                //                initialBuffersToWrite = countLastRecordBuffers - 4;
                //                ++countLastRecordBuffers;
                //            }
                // and set initialBuffersToWrite = countLastRecordBuffers - 4;
                // countLastRecordBuffers = 6, so initialBuffersToWrite = 2, which is
                // our latest buffer now and the previous from last time that we ignored while closing out the last events group.
                // so next time we do trigger and again hit if((countLastRecordBuffers >= 6) && (countLastRecordBuffers <= 8))
                // initialBuffersToWrite increases with each callback separating us from the previous events group
                // until finally we don't enter there anymore and initialBuffersToWrite remains 5 as initialized.
#ifdef DEBUG_LOGGING
                __android_log_print(ANDROID_LOG_DEBUG, __func__, "initialBuffersToWrite: %d", initialBuffersToWrite);
#endif
                int samplesToWrite = kFramesPerDataCallback * initialBuffersToWrite;
                countSamplesWrittenToDisk += fileBuffer.read(nullptr, samplesToWrite, fpAudioData); // was kMaxSamples
                // float peakAmps = fileBuffer.GetPeakAmplitude();
                // __android_log_print(ANDROID_LOG_DEBUG, __func__, "Peak amplitude of 1st event in group: %1.3f, dB: %1.1f", peakAmps, 20.f * log10(peakAmps));
                bBeganFileIO = true;
            } else
            {
                if(bAccessLog)
                {
                    if (bTriggered)
                    {
                        ShowPeak(fPeakFrequency, dB);
                        logFile.writeEvent(&mTextBuf[0], countSamplesWrittenToDisk);
                        ++countEventsWrittenToLog;
                    } else
                    {
                        logFile.writeNonevent();
                    }
                }
                countSamplesWrittenToDisk += fileBuffer.read(nullptr, kFramesPerDataCallback, fpAudioData);
            }
        }
    }else if (countLastRecordBuffers == 5)
    {
        // Clear out old data in case we get triggered again almost immediately
        // fileBuffer.clear(); Causes the screen to blank out next time around as we analyze a "silent buffer"
        // This was faulty thinking, because if we trigger again immediately, the data in the buffer is contiguous with the next fetch.
        bBeganFileIO = false;
        ++countLastRecordBuffers;
        totalEventsWrittenToLog += countEventsWrittenToLog;
        int samplesInGroup = countSamplesWrittenToDisk - nStartRecordingOffset;
        int callbacksInGroup = samplesInGroup / 4096;
        logFile.writeEndOfGroup(countEventsWrittenToLog, callbacksInGroup);
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "Stopped recording - callbacksInGroup in group: %d, countEventsWrittenToLog: %d", callbacksInGroup, countEventsWrittenToLog);
#endif
        countEventsWrittenToLog = 0;

        // Contiguous events have ended. Get the peak amplitude of all contiguous samples in the event group
        float peakAmps = fileBuffer.GetPeakAmplitude();
        if(fpEventGroupPeaks != nullptr)
        {
            fwrite(&peakAmps, sizeof(float), 1, fpEventGroupPeaks);
        }
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "Peak amplitude in group: %1.3f, dB: %1.1f", peakAmps, 20.f * log10(peakAmps));
#endif
        fileBuffer.InitPeakAmplitude();
    }

    if(bRecording)
    {
        return countSamplesWrittenToDisk;
    }

    return 0;
}


int AudioEngine::ProcessData(bool bCalledFromCode)
{
    memset(MusicalNote, 0, MUSICNOTEBUFSIZE);

    if (bStopped)
    {
        return -1;
    }

    if ((curMode == PLAY) || (curMode == EXPORT))
    {
        if (!HandlePlay(bCalledFromCode))
        {
            return 0;
        }

        if (curMode == EXPORT)
        {
            return countSamplesWrittenToDisk; // Returns offset from file, not GetSamplesPlayed();
        }
        pFFTManager->DoFFT(&mCurrentSampleData[0]);
    } else
    {
        float *pFFTData = fileBuffer.GetPtrFFTSamplesForRecord();
        pFFTManager->DoFFT(pFFTData);
    } // End if((curMode == PLAY) || (curMode == EXPORT))

    // Produce the data that the graph depends on
    char *pMusicalNote = pMusicalGraph->FindPeaks(pFFTManager, fTriggerFreqMin, fTriggerFreqMax);

    if (curMode == PLAY)
    {
        return countSamplesWrittenToDisk; // Returns offset from file, not GetSamplesPlayed();
    }

    // From here on - we are either recording or monitoring

    // New with version 1.17
    ShowPeak(pMusicalGraph->fPeakFreq, pMusicalGraph->fPeakDBs);
    if (pMusicalNote != nullptr)
    {
        strcpy(MusicalNote, pMusicalNote);
    }

    if (curMode == RECORD)
    {
        return HandleRecord(pMusicalGraph->fPeakFreq, pMusicalGraph->fPeakDBs);
    }

    return 0;
}

float AudioEngine::GetSmoothedInputLevel()
{
    // We are dealing with negative dB here
    float fSmoothedPeakAmplitude = fileBuffer.GetPeakBufferAmplitude();
    if(fSmoothedPeakAmplitude < 0.000015311)
    {
        return -96.3f;
    }
    // With input boost, amplitude could exceed 1.0,
    // but it is not yet checked for clipping at the point we are measuring it.
    return (fSmoothedPeakAmplitude >= 1.f) ? 0 : 20.f * log10(fSmoothedPeakAmplitude);
}

float AudioEngine::GetGainApplied()
{
    // We are dealing with positive dB here
    // log10(fLocalPeakAmplitude) produces a negative value,
    // so it must be negated via -20.f * log10(fLocalPeakAmplitude);
    // fScale = fLocalPeakAmplitude;
    // fScaling = fScale * 32768.f;
    // fSample = (float)16bitSample / fScaling;
    // Therefore, the smaller fScale, the smaller fScaling
    // and larger fSample
    if(fLocalPeakAmplitude >= 1.0f)
    {
        return 0;
    }
    return -20.f * log10(fLocalPeakAmplitude);
}


float AudioEngine::RenderGraph(AndroidBitmapInfo *info, void *pixels)
{
    if(bStopped || bPaused)
    {
#ifdef DEBUG_LOGGING
        __android_log_print(ANDROID_LOG_DEBUG, __func__, "bStopped | bPaused");
#endif
        return 0;
    }

    pMusicalGraph->DoGraph((uint32_t *) pixels, info->width, info->height, bRecording);
    return pMusicalGraph->fPeakDBs;
}


void AudioEngine::EraseGraph(AndroidBitmapInfo *info, void *pixels)
{
  unsigned int countPixels = (info->width * info->height);
  auto *pPixels = (uint16_t *) pixels;

/*
  uint16_t colour = 0;
  for(int y = 0; y < info->height; y++)
  {
      for(int x = 0; x < info->width; x++)
      {
          colour = pPixels[(y * info->width) + x];
          if(colour != 0)break;
      }
      if(colour != 0)break;
  }
  __android_log_print(ANDROID_LOG_DEBUG, __func__, "Graph colour: %d", colour);
*/
  memset(pPixels, 0, countPixels * 2);
#ifdef DEBUG_LOGGING
  __android_log_print(ANDROID_LOG_DEBUG, __func__, "Erased Graph!");
#endif
  if(pMusicalGraph != nullptr)
  {
      pMusicalGraph->ResetGraph();
  }
}

void AudioEngine::SetTriggerDB(float dB)
{
    fPendingTriggerDBs = dB;
    if ((curMode != RECORD) || bStopped)
    {
        fTriggerDBs = dB;
        //__android_log_print(ANDROID_LOG_DEBUG, __func__, "fTriggerDBs: %f", fTriggerDBs);
    } else if (curMode == RECORD)
    {
        // Currently running in record,
        // so update in HandleRecord()
        bTriggerUpdatePending = true;
    }
}

void AudioEngine::SetTriggerFreqs(float minFreq, float maxFreq)
{
    fPendingTriggerFreqMin = minFreq; fPendingTriggerFreqMax = maxFreq;
    if((curMode != RECORD) || bStopped)
    {
       fTriggerFreqMin = minFreq; fTriggerFreqMax = maxFreq;
      // __android_log_print(ANDROID_LOG_DEBUG, __func__, "fTriggerFreqMin: %f, fTriggerFreqMax: %f", fTriggerFreqMin, fTriggerFreqMax);
    }else if(curMode == RECORD)
    {
       bTriggerUpdatePending = true;
    }
}

// On emulator...
// Vertical   x: 1440, y: 2816, stride: 2880
// Horizontal x: 2816, y: 1440, stride: 5632

// On my phone
// Vertical   x: 1080, y: 2094, stride: 2160
/*
    for(int y = 0; y < info->height; y++)
    {
        uint32_t*  line = (uint32_t*)pixels;

        for(int x = 0; x < info->width / 2; x++)
        {
            *line++ = colour32;
        }

        // go to next line
        pixels = (char*)pixels + info->stride;
    }
    bFirstTime = !bFirstTime;
*/

/*
int AudioEngine::startThread(void)
{
    pthread_t       threadInfo_;
    pthread_attr_t  threadAttr_;

    pthread_attr_init(&threadAttr_);
    pthread_attr_setdetachstate(&threadAttr_, PTHREAD_CREATE_DETACHED);


    int result  = pthread_create( &threadInfo_, &threadAttr_, UpdateJava, this);
    assert(result == 0);

    pthread_attr_destroy(&threadAttr_);

    (void)result;

    return 0;
}
*/


/*
u_int16_t AudioEngine::RGBto565Colour(u_int8_t red, u_int8_t green, u_int8_t blue)
{
    u_int16_t r = (u_int16_t)((float)red * 0x1F / 255.f);
    u_int16_t g = (u_int16_t)((float)green * 0x3F / 255.f);
    u_int16_t b = (u_int16_t)((float)blue * 0x1F / 255.f);
    return Get565Colour(r, g, b);
}

u_int16_t AudioEngine::Get565Colour(u_int16_t red, u_int16_t green, u_int16_t blue)
{
    u_int16_t mask5 = 0x1F; // 31
    u_int16_t mask6 = 0x3F; // 63
    u_int16_t colour = ((red & mask5) << 11) | ((green & mask6) << 5) | (blue & mask5);

    return colour;
}
*/
