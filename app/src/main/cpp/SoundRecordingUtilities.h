#ifndef SOUNDRECORDINGUTILITIES_H
#define SOUNDRECORDINGUTILITIES_H

float convertInt16ToFloat(int16_t intValue);
void convertArrayInt16ToFloat(int16_t *source, float *target, int32_t length);
void fillArrayWithZeros(float *data, int32_t length);
void convertArrayMonoToStereo(float *data, int32_t numFrames);

#endif
