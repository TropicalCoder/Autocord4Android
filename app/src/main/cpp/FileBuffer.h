#ifndef SAMPLE_H
#define SAMPLE_H

#include <cstdint>
#include <array>
#include <atomic>

#include "Definitions.h"
#include "Chebyshev.h"

typedef struct filter
{
    double A[3];
    double B[3];
}FILTER, *LPFILTER;


constexpr int kFFTSize = 16384;
constexpr int kFramesPerDataCallback = 4096;
constexpr int kMaxSamples = (kFFTSize + kFramesPerDataCallback);
static const int HISTORY_BUFSIZE = 2; // For last 2 samples transformed by each filter
static const int STACK_SIZE = 3; // for 2 filters only, of which only one needs a stack

static const float alpha = 0.98;
static const float oneMinusAlpha = (1.f - alpha);


class FileBuffer
{
public:
    int32_t write(const float *sourceData, int32_t numSamples);
    int32_t read(float *targetData, int32_t numSamples, FILE *fp);
    int16_t *GetFileDataArray(void){return &mFileData[0]; };
    float *GetDataPointer(void){return &mData[0];};
    // FFT for record wants most recent data, which is found at &mData[kFramesPerDataCallback]
    // Can't adapt read() to serve both the needs of Play and Record FFT at the same time
    // so need this...
    float *GetPtrFFTSamplesForRecord(void){return &mData[kFramesPerDataCallback];};
    void clear(void);
    static const int32_t getMaxSamples() { return kMaxSamples; };
    float GetScaling(){return fScaling;}; // Only called after recording and returns scaling in normal form
    void SetScaling(float fScale){ fScaling = fScale * 32768.f;}; // Only called on Play and used for converting 16bit samples to scaled float
    float GetPeakAmplitude(){return fPeakAmplitude;}; // Get the peak amplitude of a group of contiguous events
    float GetPeakBufferAmplitude(){return fPeakBufferAmplitude;}; // Get the peak amplitude per buffer, smoothed out
    void InitPeakAmplitude(){fPeakAmplitude = 0;};  // Zero the peak amplitude prior to getting peak of a group of contiguous events
    void AttachFilter(Chebyshev *pChebyshev, int poles);
    bool bPrefilter = true;
    bool bInputBoost = true;
private:
    void FilterBuffer(const float* X, float* Y, int samples); // Causes a 7.7 dB boost in gain
    void FilterBufferCompensated(const float* X, float* Y, int samples); // Compensated for the 7.7 dB boost in gain

    std::array<float,kMaxSamples> mData { 0 };
    std::array<int16_t,kMaxSamples> mFileData { 0 };
    float Stack[STACK_SIZE] {0};
    float HistoryX[HISTORY_BUFSIZE] {0}; // History of last 2 raw samples in buffer
    float HistoryY0[HISTORY_BUFSIZE] {0}; // History of same last 2 samples transformed by filter 0
    float HistoryY1[HISTORY_BUFSIZE] {0}; // History of same last 2 samples transformed by filter 1
    float fScaling = 0; // During record, a normal value 0 to 1.0; during Play, a value from 0 to 32767
    float fPeakAmplitude = 0; // During record, a normal value 0 to 1.0, not used at any other time
    float fPeakBufferAmplitude = 0; // peak amplitude of input buffer
    float fPrevPeakBufferAmplitude = 0;

    FILTER FilterCascade[2] = {
            {0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0},
    };

//    float PrevX[DEFAULT_HISTORY_BUFSIZE] {0};
//    float PrevY[DEFAULT_HISTORY_BUFSIZE] {0};
//    int numPoles = 0;
//    int numFilters = 0;
//    int extraSamplesReqd = 0;
};

#endif // SAMPLE_H
