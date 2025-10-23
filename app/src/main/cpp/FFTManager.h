//
// Created by User on 4/14/2020.
//

#ifndef FFTMANAGER_H
#define FFTMANAGER_H

#include "FFT.h"

class FFTManager
{
public:
    FFTManager(int nSampleRate, int nFFTSize);
    ~FFTManager();

    void DoFFT(float *pData);

    void GetBinsForFrequencyRange(float fLoFreq, float fHiFreq, int* pLoBin, int* pHiBin);

    // Expect an FFT was already done
    // returns nFFTSize / 2 results in float buffer
    float *GetFFTResults(void){return pfFFTresults;};

    // Expect an FFT was already done
    // Expect n is a peak between bin 2 and bin nFFTSize / 2
    // returns magnitude -dB and peak frequency in *pPeakFreq
    // or 0 for frequency and -MaxDB if error (n was not a peak)
    float GetPeakFreq(int n, float* pPeakFreq);

    // void GetMinMaxBins(int *pMinBin, int *pMaxBin);
    float GetMaxDB(void){return (float)MaxDBs;};
    float GetFreqStep(void){return (float)dFreqStep;};

private:
    float EstimateFrequency(float* pFrequency, int k);

    FFT* pFFT;
    float *pfFFTresults;
    double const MaxDBs;
    double dFreqStep;
    int nSampleRate;
    int nFFTSize;
};


#endif // FFTMANAGER_H
