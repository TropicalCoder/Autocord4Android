//
// Created by User on 9/13/2020.
//

#include "EncodeManagerPCM.h"
#include "AudioEngine.h"

EncodeManagerPCM::EncodeManagerPCM(void* lpvEncodeBuffer, float* pDataPtr, int encodeBufferSamples, int samplesToEncode)
{
 pEncodeBuf = (int16_t*)lpvEncodeBuffer;
 pData = pDataPtr;
 nEncodeBufferSampleSize = encodeBufferSamples;
 nSamplesToEncode = samplesToEncode;
 nSamplesEncoded = 0;
 bNeedMoreData = false;
}

bool EncodeManagerPCM::needMoreData()
{
 return bNeedMoreData;
}

int EncodeManagerPCM::GetDataRead()
{
 return nSamplesEncoded;
}
// pData holds 16 K samples when we begin
// then signal bNeedMoreData = true
// Thereafter we call ProcessData for each next 4 K samples
// signalling bNeedMoreData = true for each time we complete 4K


bool EncodeManagerPCM::FillEncodeBuffer()
{
 bNeedMoreData = false;

 if(nSamplesEncoded < kFFTSize)
 {
    for (int i = 0; i < nEncodeBufferSampleSize; i++)
    {
         pEncodeBuf[i] = (int16_t) (pData[nSamplesEncoded++] * 32767.f);
    }

    if(nSamplesEncoded == kFFTSize)
    {
       bNeedMoreData = true;
    }
    return true;
 }

 for (int i = 0; i < nEncodeBufferSampleSize; i++)
 {
      pEncodeBuf[i] = (int16_t) (pData[nSamplesEncoded % 4096] * 32767.f);
      ++nSamplesEncoded;
 }

 if((nSamplesEncoded % 4096) == 0)
 {
     bNeedMoreData = true;
 }

 // Last buffer signaled by false returned.
 return nSamplesEncoded != nSamplesToEncode;
}