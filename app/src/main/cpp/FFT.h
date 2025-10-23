//
// Created by User on 4/14/2020.
//

#ifndef FFT_H
#define FFT_H

enum WINDOW {NO_WINDOW=0, HAN, BLACKMAN};

typedef struct _ComplexFloat
{
    float x;
    float y;
}ComplexFloat, *LPComplexFloat;

inline int	Round(double x) { return (int)(x+0.5); }

class FFT
{
public:
 FFT(int nFFTSize, WINDOW type);
 ~FFT();
 void DoFFT(float *pData);
 void GetFFTResult(float *pData);
private:
    void CreateFFTWindow(WINDOW type);

    void FFTR(LPComplexFloat X);
    void FFTC(LPComplexFloat X, int FFTSize);
    const double MATH_PI;
    const double TWO_PI;
    const double FOUR_PI;

    LPComplexFloat pComplexFloat;
    float *pWindow;
    int nFFTSize;
};


#endif // FFT_H
