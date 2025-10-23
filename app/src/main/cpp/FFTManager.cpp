//
// Created by User on 4/14/2020.
//
#include <cmath>
#include "FFTManager.h"

FFTManager::FFTManager(int nSampleRate, int nFFTSize)
: nSampleRate(nSampleRate), nFFTSize(nFFTSize), MaxDBs(96.3294660753), dFreqStep((double)nSampleRate / nFFTSize)
{
 pFFT = new FFT(nFFTSize, BLACKMAN);
 pfFFTresults = new float[nFFTSize / 2];
}

FFTManager::~FFTManager()
{
    delete pFFT;
    delete [] pfFFTresults;
    pFFT = nullptr;
    pfFFTresults = nullptr;
}

/*
void FFTManager::GetMinMaxBins(int *pMinBin, int *pMaxBin)
{
    *pMinBin = (int)floor(16.351597831 / dFreqStep) - 1; // 7
    *pMaxBin = (int)floor(15986.0 / dFreqStep) + 1;      // 8185
}
*/

void FFTManager::DoFFT(float *pData)
{
    pFFT->DoFFT(pData);
    pFFT->GetFFTResult(pfFFTresults);
}


float FFTManager::GetPeakFreq(int n, float* pPeakFreq)
{
    *pPeakFreq = 0;

    if ((pfFFTresults[n] > pfFFTresults[n - 1]) && (pfFFTresults[n] > pfFFTresults[n + 1]))
    {
        return EstimateFrequency(pPeakFreq, n);
    }

    return (float)-MaxDBs;
}


// 20200220 - Added this function based on the notion that the peak meter should show the peak
// within the chosen frequency range to monitor, instead of highest peak of all, but changed my mind.
// We want to see the highest peak of all to know where it is in the spectrum in order to
// set our frequency range in the first place. OTH, when we begin using the app, frequency range
// is set to full range, so we will see the highest peak of all until we change that.
// Then once we change the range, it would be easier to set the trigger if we are seeing only
// the highest peak in the range of interest. So what if something interesting happens outside
// of the range we set? We will see that in the graph at least. The other thing is, we would only
// want meter showing peak within chosen range on during record.
void FFTManager::GetBinsForFrequencyRange(float fLoFreq, float fHiFreq, int* pLoBin, int* pHiBin)
{
    float FreqStep = GetFreqStep();
    int loBin = (int)(fLoFreq / FreqStep); // round down
    //int hiBin = (int)((fHiFreq / FreqStep) + 0.5f); // round up
    int hiBin = (int)lround(fHiFreq / FreqStep); // round up
    int halfFFTSize = nFFTSize / 2;

    // Sanity check...
    if (loBin < 2)loBin = 2;
    if (hiBin >= halfFFTSize)hiBin = halfFFTSize - 1;
    if (loBin > hiBin)
    {
        loBin = 2;
        hiBin = halfFFTSize - 1;
    }
    *pLoBin = loBin;
    *pHiBin = hiBin;
}


// Ensure that iCentre represents a peak ie:  pfFFTresults[iCentre-1] < pfFFTresults[iCentre] > pfFFTresults[iCentre+1]
float FFTManager::EstimateFrequency(float* pFrequency, int k)
{
    // Equation #4 Should be many times more accurate than previous algorithm
    double P = 1.747; // was 1.75 - worse, tuned = 1.747 - better    P is a scaling constant for different windows, in this case, the Blackman
    double delta = P * (pfFFTresults[k + 1] - pfFFTresults[k - 1]) / (pfFFTresults[k] + pfFFTresults[k - 1] + pfFFTresults[k + 1]);

    *pFrequency = (float)(((double)k + delta) * dFreqStep);

    double partial = (2 * pfFFTresults[k] - pfFFTresults[k - 1] - pfFFTresults[k + 1]);
    double peak = (pfFFTresults[k] + (partial / 2.0 * delta * delta));


    if (peak < 0.000015259) // = 96.3294661 dB (-MaxDBs)
    {
        peak = -MaxDBs;
    }
    else if (peak >= 1.0)
    {
        peak = -0.000001; // UPDATE 300 - give it a very tiny negative value - see notes in MusicalSpectrum::DoGraph()
    }
    else
    {
        peak = (float)(20.0 * log10(peak));
    }

    return (float)peak;
}
