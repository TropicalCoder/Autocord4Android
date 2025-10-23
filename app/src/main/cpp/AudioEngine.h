#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#define SAMPLE_RATE 32000
#define MAXDATETIME 64

#include <cstdint>
#include <atomic>
#include <memory>
#include <aaudio/AAudio.h>
#include <android/bitmap.h>
#include <math.h>
#include <android/log.h>
#include "FileBuffer.h"
#include "FFTManager.h"
#include "MusicalGraph.h"
#include "LogFile.h"
#include "LogFileHdr.h"
#include "Oscillator.h"
#include "Chebyshev.h"
#include "EncodeManagerPCM.h"

static const int MUSICNOTEBUFSIZE = 8;
static const int MEDIAPATHNAMESIZE = 256;

enum Mode{INITIALIZE=0, MONITOR=1, RECORD=2, PLAY=3, EXPORT=4, SHUTDOWN=5};


class LogFileHdr;


class AudioEngine
{
public:

  static bool CheckExpired(void)
  {
    struct tm *today;
    time_t ltime;

    time(&ltime); // Gets time in seconds since UTC 1/1/70
    today = localtime( &ltime ); // Convert to time structure
    return false; // For 'never expires' uncomment this
    //  __android_log_print(ANDROID_LOG_ERROR, __func__, "Checking expiry date");

    // (current year minus 1900)
    if(today->tm_year > (2020 - 1900))return true;
    // January = 0
    return today->tm_mon > 10; // Currently Expires Decemeber 1st, 2010
  }

    void start(float fPeakAmplitude);
    float stop();
    void restart();
    void openFile(char *pPathname, int fileSizeBytes);
    aaudio_data_callback_result_t recordingCallback(float *audioData, int32_t numFrames);
    aaudio_data_callback_result_t playbackCallback(float *audioData, int32_t numFrames);
    void SetMode(int nMode);
    void EnablePause(bool bEnable){bPaused = bEnable;};
    void EnablePrefilter(bool bEnable){fileBuffer.bPrefilter = bEnable;};
    void EnableInputBoost(bool bEnable){fileBuffer.bInputBoost = bEnable;};
    char *GetTimeRecorded(void);
    float GetSmoothedInputLevel();
    float GetGainApplied();
    float GetMusicalNote(int x);
    int GetFrequencyMusicalNoteArrayOffset(float f);
    void SetTriggerDB(float dB);
    void SetTriggerFreqs(float minFreq, float maxFreq);
    int ProcessData(bool bFetchPeak);
    float RenderGraph(AndroidBitmapInfo*  info, void*  pixels);
    void EraseGraph(AndroidBitmapInfo*  info, void*  pixels);
    static void CacheCallbackParams(JavaVM *vm, jclass claz, JNIEnv* environ);
    char *getTimeStr(void){return TimeBuf;};
    char *GetPeakInfo(void){return &mTextBuf[0];};
    char *GetMusicalNote(void){return &MusicalNote[0];};
    int GetTotalEventsWrittenToLog(){return totalEventsWrittenToLog;};
    void signalDataReady(int dataExported);
    void passEncodeBuffer(__uint8_t *pBuffer, int bufLen){pEncodeBuffer = pBuffer; nEncodeBufSize = bufLen;};
    bool PrepareEncodeBuffer();
    FileBuffer fileBuffer;
    int currentAudioFileSampleLength = 0;
    int currentAudioFileRemainingSamples = 0;
    int countCallbacks = 0;
    char MediaPathname[MEDIAPATHNAMESIZE];
    __uint8_t * pEncodeBuffer = nullptr;
    int nEncodeBufSize = 0;

private:
    FFTManager *pFFTManager = nullptr;
    MusicalSpectrum *pMusicalGraph = nullptr;
    LogFile logFile;
    Oscillator oscillator;
    AAudioStream* mPlaybackStream = nullptr;
    AAudioStream* mRecordingStream = nullptr;
    char TimeBuf[TIMEBUFSIZE];
    char MusicalNote[MUSICNOTEBUFSIZE];
    char mTextBuf[TEXTBUFSIZE];
    std::array<float, kMaxSamples> mCurrentSampleData { 0 };
    std::array<float, kFFTSize> mPreviousSampleData { 0 };

    FILE *fpAudioData = NULL;
    FILE *fpEventGroupPeaks = NULL;
    float fGlobalPeakAmplitude = 0; // Saved peak amplitude passed on Start()
    float fLocalPeakAmplitude = 0; // peak amplitude of contiguous events group
    float fBufferPeakAmplitude = 0; // peak amplitude of input buffer
    float fTriggerFreqMin {16.0f};
    float fTriggerFreqMax {16000.0f};
    float fTriggerDBs {-97.3f};
    float fPendingTriggerFreqMin {16.0f};
    float fPendingTriggerFreqMax {16000.0f};
    float fPendingTriggerDBs {-97.3f};

    Mode curMode{MONITOR};
    int nBuffersInPreroll = 0;
    int countNVTs = 0;
    // dbg only!
    int countLastRecordBuffers = 0;
    int countSamplesWrittenToDisk = 0;
    int countEventsWrittenToLog = 0;
    int totalEventsWrittenToLog = 0;
    int nStartRecordingOffset = 0;
    int countContiguousSamplesGroups = 0; // for logging & dbg info only!
    bool bTriggerUpdatePending = false;
    bool bInitialized = false;
    bool bAccessLog = false;
    bool bBeganFileIO = false;
    bool bRecording = false;
    bool bPaused = false;
    bool bStopped = true;
    bool bRewound = false;
    bool bAttached = false;

    void OpenExportTextFile(char *pPathname, int fileSizeBytes);
    int GetSamplesPlayed(void);
    void FetchPeak(bool bRewind);
    void ShowPeak(float fPeakFrequency, float dB);
    bool PlayCallbackFetchesAudioData(int nSamplesToFetch);
    bool HandlePlay(bool bCalledFromCode);
    int HandleRecord(float fPeakFrequency, float dB);
    void startRecord(int deviceID);
    void startPlay();
    void InitVarsOnStartRecord();
    void stopStream(AAudioStream *stream) const;
    static void closeStream(AAudioStream **stream);
    void closeFile();
    // int startThread(void);
    EncodeManagerPCM *pEncodeManager;
    // EncodeManagerFloat *pEncodeManager;
};


#endif //PART1_AUDIOENGINE_H
