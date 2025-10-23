//
// Created by User on 4/14/2020.
//
#include <cmath>
#include <cstring>
#include "FFT.h"

FFT::FFT(int nFFTSize, WINDOW type) : nFFTSize(nFFTSize), MATH_PI(3.14159265359), TWO_PI(6.28318530718), FOUR_PI(12.56637061436)
{
    CreateFFTWindow(type);
    pComplexFloat = new ComplexFloat[nFFTSize];
}

FFT::~FFT()
{
    if(pWindow != nullptr)
    {
        delete [] pWindow;
        pWindow = nullptr;
    }
    delete [] pComplexFloat;
    pComplexFloat = nullptr;
}

void FFT::CreateFFTWindow(WINDOW type)
{
    if (type == NO_WINDOW)
    {
        pWindow = nullptr;
        return;
    }

    int points = nFFTSize + 1;
    int i = 1; // starting at 1

    pWindow = new float[nFFTSize];

    int n;
    double frac;
    if (type == HAN)
    {
        for (n = 0; n < nFFTSize; n++)
        {
            frac = i++ / (double)points;
            pWindow[n] = (float)(0.5 * (1.0 - cos(TWO_PI * frac)));
        }
    }
    else if (type == BLACKMAN)
    {
        for (n = 0; n < nFFTSize; n++)
        {
            frac = i++ / (double)points;
            pWindow[n] = (float)(0.42 - 0.5 * cos(TWO_PI * frac) + 0.08 * cos(FOUR_PI * frac));
        }
    }
}

void FFT::DoFFT(float *pData)
{
  // FFT requires we pack the data into the complex buffer as if it were a float array
  auto *pBuf = (float*)pComplexFloat;

  memset(&pBuf[nFFTSize], 0, sizeof(float) * nFFTSize);
  memcpy(pBuf, pData, sizeof(float) * nFFTSize);

  if(pWindow != nullptr)
  {
     for(int i = 0; i < nFFTSize; i++)
     {
         pBuf[i] *= pWindow[i];
     }
  }

  FFTR(pComplexFloat);
}

void FFT::GetFFTResult(float *pData)
{
    double M = 2.38 / (nFFTSize / 2); // 2.38 - correction factor for BLACKMAN window at SAMPLE_RATE 32000

    *pData = 0;
    for(int i = 1; i < (nFFTSize / 2); i++)
    {
        pData[i] = (float)(M * hypotf(pComplexFloat[i].x, pComplexFloat[i].y));
    }
}


// Real FFT
// Input: N real points at x, anything at y
// Output: N/2+1 real freqs, N/2+1 imaginary freqs
void FFT::FFTR(LPComplexFloat X)
{
    //	_complex *ptrX, *ptrXend;
    int	N, ND2, ND4;
    int	i, im, ip2, ipm, ip;
    float UR, UI, SR, SI, TR, TI;

    // Separate even and odd points
    N = nFFTSize;
    ND2 = N >> 1;
    ND4 = ND2 >> 1;
    //******************************
    // Calculate N/2 point FFT

    FFTC(X, ND2);
    // Even/odd frequency domain decomposition
    for (i = 1; i < ND4; i++)
    {
        im = ND2 - i;
        ip2 = i + ND2;
        ipm = im + ND2;
        (X + ipm)->x = (X + ip2)->x = ((X + i)->y + (X + im)->y) * 0.5f;
        (X + ip2)->y = ((X + i)->x - (X + im)->x) * (-0.5f);
        (X + ipm)->y = -(X + ip2)->y;
        (X + im)->x = (X + i)->x = ((X + i)->x + (X + im)->x) * 0.5f;
        (X + i)->y = ((X + i)->y - (X + im)->y) * 0.5f;
        (X + im)->y = -(X + i)->y;
    }
    (X + N * 3 / 4)->x = (X + ND4)->y;
    (X + ND2)->x = X->y;
    (X + ND2 + ND4)->y = (X + ND2)->y = (X + ND4)->y = X->y = 0;
    // Complete the last FFT stage
    // First step: calculate X[0] and X[N/2]
    TR = (X + ND2)->x;
    TI = (X + ND2)->y;
    (X + ND2)->x = X->x - TR;
    (X + ND2)->y = X->y - TI;
    X->x = X->x + TR;
    X->y = X->y + TI;
    // Other steps
    UR = SR = (float)cos(MATH_PI / ND2);
    UI = SI = (float)-sin(MATH_PI / ND2);
    ip = ND2 + 1;
    for (i = 1; i < ND2; i++, ip++)
    {
        (X + i)->x = (X + i)->x + (X + ip)->x * UR - (X + ip)->y * UI;
        (X + i)->y = (X + i)->y + (X + ip)->x * UI + (X + ip)->y * UR;
        TR = UR;
        UR = TR * SR - UI * SI;
        UI = TR * SI + UI * SR;
    }
}


// Complex FFT
// Input: N complex points of time domain
// Output: N complex points of frequency domain
void FFT::FFTC(LPComplexFloat X, int FFTSize)
{
    int	N, M, i, j, L, LE, LE2, ip, k, s;
    ComplexFloat t, z;
    float UR, UI, SR, SI, TR, TI;

    N = FFTSize;
    M = Round(log((double)N) / log(2.0));
    // Bit-reverse
    i = 0;
    for (s = 0; s < N - 1; s++) {
        if (s < i) {
            t = *(X + i); *(X + i) = *(X + s); *(X + s) = t;
        }
        k = N >> 1;
        while (i & k) k >>= 1;
        i += k;
        k <<= 1;
        while (k < N) {
            i -= k;
            k <<= 1;
        }
    }
    // First pass
    for (i = 0; i < N; i += 2) {
        t = *(X + i);
        (X + i)->x = t.x + (X + i + 1)->x;
        (X + i)->y = t.y + (X + i + 1)->y;
        (X + i + 1)->x = t.x - (X + i + 1)->x;
        (X + i + 1)->y = t.y - (X + i + 1)->y;
    }
    // Second pass
    for (i = 0; i < N; i += 4) {
        t = *(X + i);
        (X + i)->x = t.x + (X + i + 2)->x;
        (X + i)->y = t.y + (X + i + 2)->y;
        (X + i + 2)->x = t.x - (X + i + 2)->x;
        (X + i + 2)->y = t.y - (X + i + 2)->y;
        t = *(X + i + 1);
        z = *(X + i + 3);
        (X + i + 1)->x = t.x + z.y;
        (X + i + 1)->y = t.y - z.x;
        (X + i + 3)->x = t.x - z.y;
        (X + i + 3)->y = t.y + z.x;
    }
    // Last passes
    for (L = 3; L <= M; L++) {
        LE = 1 << L;
        LE2 = LE >> 1;
        UR = 1; UI = 0;
        SR = (float)cos(MATH_PI / LE2);
        SI = (float)-sin(MATH_PI / LE2);
        for (j = 0; j < LE2; j++) {
            for (i = j; i < N; i += LE) {
                ip = i + LE2;
                TR = (X + ip)->x * UR - (X + ip)->y * UI;
                TI = (X + ip)->x * UI + (X + ip)->y * UR;
                (X + ip)->x = (X + i)->x - TR;
                (X + ip)->y = (X + i)->y - TI;
                (X + i)->x = (X + i)->x + TR;
                (X + i)->y = (X + i)->y + TI;
            }
            TR = UR;
            UR = TR * SR - UI * SI;
            UI = TR * SI + UI * SR;
        }
    }
}
