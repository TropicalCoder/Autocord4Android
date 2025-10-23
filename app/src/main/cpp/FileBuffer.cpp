
#include <android/log.h>
#include <cmath>
#include "FileBuffer.h"

int32_t FileBuffer::write(const float *sourceData, int32_t numSamples)
{
    if(sourceData == nullptr)
    {
       // We are in play mode, converting file data to float for later fetches by callback
       // Data has been read from file as 16 bit samples and stored in mFileData[] for us
       // PlayCallbackFetchesAudioData() calls fileBuffer.write(nullptr, nSamplesToFetch);
       // numSamples will be kFFTSize when we begin
       // and thereafter numSamples = kFramesPerDataCallback
       if(numSamples == kFramesPerDataCallback)
       {
           // Need to move data down in the buffer and fill in front of buffer with new data
           memmove(&mData[kFramesPerDataCallback], &mData[0], sizeof(float) * kFFTSize);
       }

       for(int j = 0; j < numSamples; j++)
       {
           float fSample = (float)mFileData[j] / fScaling;
           if(fSample > 1.0f)
           {
               fSample = 1.0f;
           }else if(fSample < -1.0f)
           {
               fSample = -1.0f;
           }
           mData[j] = fSample;
       }
    } else
    {
      // We are in record or monitor mode.
      // Only Record callback calls fileBuffer.write(audioData, numFrames);
      // Move the old data up to make room for new...
      // *Note - expects that numSamples == kFramesPerDataCallback!
      // We are moving data in the opposite direction we do in Play
      // That is because we always want the oldest data at front of buffer,
      // moving latest to end of buffer, so that at any time when the latest data triggers,
      // we have 4 buffers of data that preceded that point for the "half second of data before the event".
      memmove(&mData[0], &mData[kFramesPerDataCallback], sizeof(float) * kFFTSize);
      // memcpy(&mData[kFFTSize], sourceData, sizeof(float) * numSamples);

      fPeakBufferAmplitude = 0;

      if(bPrefilter)
      {
          // Filter provides a 7.7 dB boost at 440 Hz
          // (Tested with 440 Hz sinewave at -20.0 dB,
          //  resulted in peak sample 0.100 amplitude/-20.0 dB unfiltered,
          //                     and  0.242 amplitude/-12.3 dB filtered)
          if(bInputBoost)
          {
              FilterBuffer(const_cast<float *>(sourceData), &mData[kFFTSize], numSamples);
          } else
          {
              FilterBufferCompensated(const_cast<float *>(sourceData), &mData[kFFTSize], numSamples);
          }
      } else
      {
          if(bInputBoost)
          {
              for (int i = 0; i < numSamples; i++)
              {
                  float fSample = sourceData[i] * 2.4266100950824f; // -7.7 dB
                  mData[i + kFFTSize] = fSample;
                  if(fabs(fSample) > fPeakBufferAmplitude)fPeakBufferAmplitude = fabs(fSample);
              }
          } else
          {
              for (int i = 0; i < numSamples; i++)
              {
                  float fSample = sourceData[i];
                  mData[i + kFFTSize] = fSample;
                  if(fabs(fSample) > fPeakBufferAmplitude)fPeakBufferAmplitude = fabs(fSample);
              }
          }
      }

      // Do some kind of smoothing to serve as VU meter
      fPeakBufferAmplitude = (fPrevPeakBufferAmplitude * 0.6f) + (fPeakBufferAmplitude * 0.4f);
      fPrevPeakBufferAmplitude = fPeakBufferAmplitude;
    }

    return numSamples;
}

// On record...
// Normally expecting (fpAudioData == NULL) and (numSamples == kFFTSize)
// When we want to begin write we want the approximately the half second of data
// before the event that that triggered disk write, along with the latest data.
// So disk write will begin with numSamples == kMaxSamples
// and numSamples == kFramesPerDataCallback thereafter
int32_t FileBuffer::read(float *targetData, int32_t numSamples, FILE *fp)
{
    auto retval = static_cast<size_t>(numSamples);
    if(fp == nullptr)
    {
       // We are fetching data for EXPORT (1st fetch) or the FFT (numSamples == kFFTSize),
       // or for EXPORT or play callback (numSamples == kFramesPerDataCallback)
       // On RECORD we copy data for FFT with dedicated code in ProcessData(),
       // because that wants the data starting at &mData[kFramesPerDataCallback]
       memcpy(targetData, &mData[0], sizeof(float) * numSamples);
    } else
    {
       // We want to write the data to disk, first converting the floats to int16s
       // followed by a disk write at the bottom.
       // First disk write requests up to all the data we have (numSamples == kMaxSamples)
       // When triggered to begin a new event in HandleRecord() in if(!bBeganFileIO)
       // int samplesToWrite = kFramesPerDataCallback * initialBuffersToWrite;
       // calls fileBuffer.read(nullptr, samplesToWrite, fpAudioData)
       // Subsequently call fileBuffer.read(nullptr, kFramesPerDataCallback, fpAudioData)
       // for as many events & non-events until end of group
       // Subsequent disk writes only want the latest samples
       // from the end of the buffer (numSamples == kFramesPerDataCallback)
       int nBufferReadOffset = kMaxSamples - numSamples;
       for(int i = 0; i < numSamples; i++)
       {
           float fSample = mData[nBufferReadOffset + i];

           // Need to check for clipping!
           if(fSample > 1.0f)
           {
               fSample = 1.0f;
           }else if(fSample < -1.0f)
           {
               fSample = -1.0f;
           }

           // At the moment, fPeakAmplitude will store the peak of each contiguous events group
           // while fScaling will store the peak of all recorded events
           // Then we will be able to perform normalization by contiguous events group,
           // and fScaling may be just for statistical purposes or backup.
           // Get the peak amplitude in this buffer

           // Added with version 1.15 - Sept 19, 2020
           // We now have 3 kinds of peak amplitue here - ignoring the peak amplitude
           // of peak frequency we get from the FFT.
           // We want the peak for current buffer, from which we will power a kind of "VU meter"
           // We want the peak for current contigous samples group, for later normalization of that group
           // We want the peak for the entire recording session, which is used as back up in case
           // for some reason we cannot retrieve the peaks for the contigous samples groups
           // Today I am adding fPeakBufferAmplitude - up above on incomming audio

           if(fabs(fSample) > fPeakAmplitude)fPeakAmplitude = fabs(fSample);

           mFileData[i] = (int16_t)(fSample * 32767.f);
       }

       if(fPeakAmplitude > fScaling)fScaling = fPeakAmplitude;


       retval = fwrite(&mFileData[0], sizeof(int16_t), static_cast<size_t>(numSamples), fp);
    }

    return retval; // The total number of elements sizeof(int16_t) successfully written is returned.
}

void FileBuffer::clear()
{
    fPrevPeakBufferAmplitude = 0;
    memset(&mData[0], 0, sizeof(float) * kMaxSamples);
    memset(Stack, 0, sizeof(float) * STACK_SIZE);
    memset(HistoryX, 0, sizeof(float) * HISTORY_BUFSIZE); // History of last 2 raw samples in buffer
    memset(HistoryY0, 0, sizeof(float) * HISTORY_BUFSIZE); // History of same last 2 samples transformed by filter 0
    memset(HistoryY1, 0, sizeof(float) * HISTORY_BUFSIZE); // History of same last 2 samples transformed by filter 1

    // memset(PrevX, 0, sizeof(float) * extraSamplesReqd);
    // memset(PrevY, 0, sizeof(float) * extraSamplesReqd);
}

// If at some point I may call this with different values for 'poles'
// I need to delete and reallocate the memory allocated below
void FileBuffer::AttachFilter(Chebyshev *pChebyshev, int poles)
{
    int numPoles = poles;
    // numFilters = numPoles / 2;
/*
    __android_log_print(ANDROID_LOG_DEBUG, __func__,
                        "Attaching filter, poles: %d, numFilters: %d",
                        poles, numFilters);
*/
    for (int poleNum = 1; poleNum <= (numPoles / 2); poleNum++)
    {
        pChebyshev->CalculateCascade(FilterCascade[poleNum - 1].A, FilterCascade[poleNum - 1].B, poleNum);
    }
}

void FileBuffer::FilterBuffer(const float* X, float* Y, int samples)
{
// Defines to be better able to see the subscripts
// for pFilterCascade and History.
// They match up with the index.
#define J 0 // match to index j
#define K 1 // match to index k

    int j, k;

    j = 0;
    Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)HistoryX[0] + FilterCascade[J].A[2] * (double)HistoryX[1]
           + FilterCascade[J].B[1] * (double)HistoryY0[0] + FilterCascade[J].B[2] * (double)HistoryY0[1]);

    j = 1;
    Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)X[j - 1] + FilterCascade[J].A[2] * (double)HistoryX[0]
           + FilterCascade[J].B[1] * (double)Y[j - 1] + FilterCascade[J].B[2] * (double)HistoryY0[0]);

    j = 2;
    Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)X[j - 1] + FilterCascade[J].A[2] * (double)X[j - 2]
           + FilterCascade[J].B[1] * (double)Y[j - 1] + FilterCascade[J].B[2] * (double)Y[j - 2]);

    j = 3;
    Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)X[j - 1] + FilterCascade[J].A[2] * (double)X[j - 2]
                   + FilterCascade[J].B[1] * (double)Y[j - 1] + FilterCascade[J].B[2] * (double)Y[j - 2]);


    k = 0;

    // Stack for history of what the last 2 samples were _before_ they were overwritten
    Stack[2] = HistoryY0[1];
    Stack[1] = HistoryY0[0];
    Stack[0] = Y[k];
    Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
           + FilterCascade[K].B[1] * (double)HistoryY1[0] + FilterCascade[K].B[2] * (double)HistoryY1[1]);

    if(fabs(Y[k]) > fPeakBufferAmplitude)fPeakBufferAmplitude = fabs(Y[k]);

    j = 4;
    Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)X[j - 1] + FilterCascade[J].A[2] * (double)X[j - 2]
                   + FilterCascade[J].B[1] * (double)Y[j - 1] + FilterCascade[J].B[2] * (double)Y[j - 2]);


    k = 1;
    Stack[2] = Stack[1];
    Stack[1] = Stack[0];
    Stack[0] = Y[k];
    Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
           + FilterCascade[K].B[1] * (double)Y[k - 1] + FilterCascade[K].B[2] * (double)HistoryY1[0]);

    if(fabs(Y[k]) > fPeakBufferAmplitude)fPeakBufferAmplitude = fabs(Y[k]);

    j = 5;
    k = 2;
    for (; j < samples; j++, k++)
    {
        Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)X[j - 1] + FilterCascade[J].A[2] * (double)X[j - 2]
                       + FilterCascade[J].B[1] * (double)Y[j - 1] + FilterCascade[J].B[2] * (double)Y[j - 2]);

        Stack[2] = Stack[1];
        Stack[1] = Stack[0];
        Stack[0] = Y[k];
        Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
               + FilterCascade[K].B[1] * (double)Y[k - 1] + FilterCascade[K].B[2] * (double)Y[k - 2]);

        if(fabs(Y[k]) > fPeakBufferAmplitude)fPeakBufferAmplitude = fabs(Y[k]);
    }

    // Now save history for next buffer
    for (int i = 0; i < HISTORY_BUFSIZE; i++)
    {
        --j;
        HistoryX[i] = X[j];
        HistoryY0[i] = Y[j];
    }

    // Now we can finish off the current buffer
    Stack[2] = Stack[1];
    Stack[1] = Stack[0];
    Stack[0] = Y[k];
    Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
                   + FilterCascade[K].B[1] * (double)Y[k - 1] + FilterCascade[K].B[2] * (double)Y[k - 2]);

    if(fabs(Y[k]) > fPeakBufferAmplitude)fPeakBufferAmplitude = fabs(Y[k]);

    ++k;
    Stack[2] = Stack[1];
    Stack[1] = Stack[0];
    Stack[0] = Y[k];
    Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
                   + FilterCascade[K].B[1] * (double)Y[k - 1] + FilterCascade[K].B[2] * (double)Y[k - 2]);

    if(fabs(Y[k]) > fPeakBufferAmplitude)fPeakBufferAmplitude = fabs(Y[k]);

    ++k;
    Stack[2] = Stack[1];
    Stack[1] = Stack[0];
    Stack[0] = Y[k];
    Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
                   + FilterCascade[K].B[1] * (double)Y[k - 1] + FilterCascade[K].B[2] * (double)Y[k - 2]);

    if(fabs(Y[k]) > fPeakBufferAmplitude)fPeakBufferAmplitude = fabs(Y[k]);

    ++k;
    for (int i = 0; i < HISTORY_BUFSIZE; i++)
    {
        --k;
        HistoryY1[i] = Y[k];
    }
}

void FileBuffer::FilterBufferCompensated(const float *X, float *Y, int samples)
{
// Defines to be better able to see the subscripts
// for pFilterCascade and History.
// They match up with the index.
#define J 0 // match to index j
#define K 1 // match to index k

    int j, k;

    j = 0;
    Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)HistoryX[0] + FilterCascade[J].A[2] * (double)HistoryX[1]
                   + FilterCascade[J].B[1] * (double)HistoryY0[0] + FilterCascade[J].B[2] * (double)HistoryY0[1]);

    j = 1;
    Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)X[j - 1] + FilterCascade[J].A[2] * (double)HistoryX[0]
                   + FilterCascade[J].B[1] * (double)Y[j - 1] + FilterCascade[J].B[2] * (double)HistoryY0[0]);

    j = 2;
    Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)X[j - 1] + FilterCascade[J].A[2] * (double)X[j - 2]
                   + FilterCascade[J].B[1] * (double)Y[j - 1] + FilterCascade[J].B[2] * (double)Y[j - 2]);

    j = 3;
    Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)X[j - 1] + FilterCascade[J].A[2] * (double)X[j - 2]
                   + FilterCascade[J].B[1] * (double)Y[j - 1] + FilterCascade[J].B[2] * (double)Y[j - 2]);


    k = 0;

    // Stack for history of what the last 2 samples were _before_ they were overwritten
    Stack[2] = HistoryY0[1];
    Stack[1] = HistoryY0[0];
    Stack[0] = Y[k];
    Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
                   + FilterCascade[K].B[1] * (double)HistoryY1[0] + FilterCascade[K].B[2] * (double)HistoryY1[1]);


    j = 4;
    Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)X[j - 1] + FilterCascade[J].A[2] * (double)X[j - 2]
                   + FilterCascade[J].B[1] * (double)Y[j - 1] + FilterCascade[J].B[2] * (double)Y[j - 2]);


    k = 1;
    Stack[2] = Stack[1];
    Stack[1] = Stack[0];
    Stack[0] = Y[k];
    Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
                   + FilterCascade[K].B[1] * (double)Y[k - 1] + FilterCascade[K].B[2] * (double)HistoryY1[0]);

    j = 5;
    k = 2;
    for (; j < samples; j++, k++)
    {
        Y[j] = (float)(FilterCascade[J].A[0] * (double)X[j] + FilterCascade[J].A[1] * (double)X[j - 1] + FilterCascade[J].A[2] * (double)X[j - 2]
                       + FilterCascade[J].B[1] * (double)Y[j - 1] + FilterCascade[J].B[2] * (double)Y[j - 2]);

        Stack[2] = Stack[1];
        Stack[1] = Stack[0];
        Stack[0] = Y[k];
        Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
                       + FilterCascade[K].B[1] * (double)Y[k - 1] + FilterCascade[K].B[2] * (double)Y[k - 2]);

        Y[k - 2] *= 0.4120975191; // -7.7 dB

        if(fabs(Y[k - 2]) > fPeakBufferAmplitude)fPeakBufferAmplitude = fabs(Y[k - 2]);
    }

    int last = k - 2;

    // Now save history for next buffer
    for (int i = 0; i < HISTORY_BUFSIZE; i++)
    {
        --j;
        HistoryX[i] = X[j];
        HistoryY0[i] = Y[j];
    }

    // Now we can finish off the current buffer
    Stack[2] = Stack[1];
    Stack[1] = Stack[0];
    Stack[0] = Y[k];
    Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
                   + FilterCascade[K].B[1] * (double)Y[k - 1] + FilterCascade[K].B[2] * (double)Y[k - 2]);


    ++k;
    Stack[2] = Stack[1];
    Stack[1] = Stack[0];
    Stack[0] = Y[k];
    Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
                   + FilterCascade[K].B[1] * (double)Y[k - 1] + FilterCascade[K].B[2] * (double)Y[k - 2]);

    ++k;
    Stack[2] = Stack[1];
    Stack[1] = Stack[0];
    Stack[0] = Y[k];
    Y[k] = (float)(FilterCascade[K].A[0] * (double)Stack[0] + FilterCascade[K].A[1] * (double)Stack[1] + FilterCascade[K].A[2] * (double)Stack[2]
                   + FilterCascade[K].B[1] * (double)Y[k - 1] + FilterCascade[K].B[2] * (double)Y[k - 2]);

    ++k;
    for (int i = 0; i < HISTORY_BUFSIZE; i++)
    {
        --k;
        HistoryY1[i] = Y[k];
    }

    for(; last < samples; last++)
    {
        Y[last] *= 0.4120975191; // -7.7 dB
        if(fabs(Y[last]) > fPeakBufferAmplitude)fPeakBufferAmplitude = fabs(Y[last]);
    }
}

/*
void FileBuffer::FilterBuffer(float* X, float* Y, int samples)
{
    int n = 0;
    // We have pPrev[extraSamplesReqd] X & Y from the tail of the previous buffer

    int j = 0;
    Y[j] = pFilterCascade[n]->A[0] * X[j] + pFilterCascade[n]->A[1] * PrevX[0] + pFilterCascade[n]->A[2] * PrevX[1]
           + pFilterCascade[n]->B[1] * PrevY[0] + pFilterCascade[n]->B[2] * PrevY[1];

    j = 1;
    Y[j] = pFilterCascade[n]->A[0] * X[j] + pFilterCascade[n]->A[1] * X[j - 1] + pFilterCascade[n]->A[2] * PrevX[0]
           + pFilterCascade[n]->B[1] * Y[j - 1] + pFilterCascade[n]->B[2] * PrevY[0];

    j = 2;
    for (; j < samples; j++)
    {
        Y[j] = pFilterCascade[n]->A[0] * X[j] + pFilterCascade[n]->A[1] * X[j - 1] + pFilterCascade[n]->A[2] * X[j - 2]
               + pFilterCascade[n]->B[1] * Y[j - 1] + pFilterCascade[n]->B[2] * Y[j - 2];
    }

    // Now save history for next buffer
    for (int i = 0; i < extraSamplesReqd; i++)
    {
        --j;
        PrevX[i] = X[j];
        PrevY[i] = Y[j];
    }
}
*/
