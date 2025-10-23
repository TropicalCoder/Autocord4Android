//
// Created by User on 9/13/2020.
//

#ifndef AUTOCORD_ENCODEMANAGERPCM_H
#define AUTOCORD_ENCODEMANAGERPCM_H


#include <cstdint>

class EncodeManagerPCM
{
public:
    EncodeManagerPCM(void* lpvEncodeBuffer, float* pDataPtr, int encodeBufferSamples, int samplesToEncode);

    // Returns false when done - a signal to employer to delete this class.
    bool FillEncodeBuffer();
    bool needMoreData();
    int GetDataRead();

private:
    float* pData;
    int16_t* pEncodeBuf;
    int nSamplesToEncode;
    int nSamplesEncoded;
    int nEncodeBufferSampleSize;
    bool bNeedMoreData;
};


#endif //AUTOCORD_ENCODEMANAGERPCM_H
