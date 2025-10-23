#ifndef OSCILLATOR_H
#define OSCILLATOR_H

#include <atomic>
#include <stdint.h>
#define AMPLITUDE 0.1 // 0.316227766 // Down 10 dB
#define FREQUENCY 440.0

class Oscillator {
public:
    void setWaveOn(bool isWaveOn);
    void reset();
    void setSampleRate(int32_t sampleRate);

    void render(float *audioData, int32_t numFrames);
    void DecrementFrequency();
    void IncrementFrequency();
private:
    // We use an atomic bool to define isWaveOn_ because it is accessed from multiple threads.
    std::atomic<bool> isWaveOn_{false};
    double dSampleRate;
    double phase_ = 0.0;
    double phaseIncrement_ = 0.0;
    double frequency = FREQUENCY;
};


#endif
