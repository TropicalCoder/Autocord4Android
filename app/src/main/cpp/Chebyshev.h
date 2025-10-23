#ifndef CHEBYSHEV_H
	#define CHEBYSHEV_H

#define DEFAULT_CUTOFF_FREQUENCY 90.0
#define DEFAULT_PERCENT_RIPPLE 0.5 // Recommended
#define DEFAULT_NUM_POLES 4
#define DEFAULT_NUM_FILTER (DEFAULT_NUM_POLES / 2)
#define DEFAULT_HISTORY_BUFSIZE (DEFAULT_NUM_FILTER * 3)
#define LOPASS (false)
#define HIPASS (true)
#define MAX_COEFFICIENTS 22
// The cutoff frequency, FC, is expressed as a fraction of the sampling frequency,
// and therefore must be in the range: 0 to 0.5.
// The variable bType, is set to a value of true for a high-pass filter, and false for a low-pass filter.
// The value entered for PR must be in the range of 0 to 29, corresponding to 0 to 29% ripple in the filter's frequency response.
// The number of poles in the filter, entered in the variable NP, must be an even integer between 2 and 20.

class Chebyshev
{
public:
 Chebyshev(double dCentreFrequency, double nPercentRipple, int nNumPoles, bool bFilterType);
 ~Chebyshev();
 void CalculateCoefficients(void);
 void CalculateCascade(double *pA, double *pB, int poleNum);
 void GetFilter(double *pA, double *pB);
private:
 double *pCoeffA; // [MAX_COEFFICIENTS]; // the "a" recursion coefficients
 double *pCoeffB; // [MAX_COEFFICIENTS]; // the "b" recursion coefficients
 double *pTA; // [MAX_COEFFICIENTS]; // internal use for combining stages
 double *pTB; // [MAX_COEFFICIENTS]; // internal use for combining stages
 double fc;
 double pr;
 int np;
 bool bType;
};
#endif
