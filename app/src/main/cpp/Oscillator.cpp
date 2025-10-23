#include "Oscillator.h"
#include <cmath>

#define TWO_PI 6.28318530718

void Oscillator::setSampleRate(int32_t sampleRate)
{
    dSampleRate = sampleRate;
    frequency = FREQUENCY;
    phase_ = 0;
    phaseIncrement_ = (TWO_PI * frequency) / dSampleRate;
}

void Oscillator::reset()
{
    frequency = FREQUENCY;
    phase_ = 0;
}

void Oscillator::DecrementFrequency()
{
    if(frequency > 10.0)
    {
       frequency -= 5.0;
    }
    phase_ = 0;
    phaseIncrement_ = (TWO_PI * frequency) / dSampleRate;
}

void Oscillator::IncrementFrequency()
{
    if(frequency < 15800.0)
    {
        frequency += 2.0;
    }
    phase_ = 0;
    phaseIncrement_ = (TWO_PI * frequency) / dSampleRate;
}

void Oscillator::setWaveOn(bool isWaveOn)
{
/*
    if (isWaveOn)
    {
        __android_log_write(ANDROID_LOG_VERBOSE, "Oscillator::setWaveOn", "Enabled tone");
    } else
    {
        __android_log_write(ANDROID_LOG_VERBOSE, "Oscillator::setWaveOn", "Disabled tone");
    }
*/
    isWaveOn_.store(isWaveOn);
}

void Oscillator::render(float *audioData, int32_t numFrames)
{

    // If the wave has been switched off then reset the phase to zero. Starting at a non-zero value
    // could result in an unwanted audible 'click'
    if (!isWaveOn_.load()) phase_ = 0;

    for (int i = 0; i < numFrames; i++)
    {

        if (isWaveOn_.load())
        {

            // Calculates the next sample value for the sine wave.
            audioData[i] = (float) (sin(phase_) * AMPLITUDE);

            // Increments the phase, handling wrap around.
            phase_ += phaseIncrement_;
            if (phase_ > TWO_PI) phase_ -= TWO_PI;

        } else
        {
            // Outputs silence by setting sample value to zero.
            audioData[i] = 0;
        }
    }
}
