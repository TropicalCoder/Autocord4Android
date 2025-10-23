#include "Chebyshev.h"
#include <cmath>
#include <cstring>

#define PI 3.14159265358979323846

// 29 April 2012 - dCentreFrequency is expressed as desired frequency / sample rate
// ie: desired frequency = 3,000 Hz, sample rate = 44,100. dCentreFrequency = 3000/44100 = 0.06802721088
Chebyshev::Chebyshev(double dCentreFrequency, double nPercentRipple, int nNumPoles, bool bFilterType)
{
 fc = dCentreFrequency, np = nNumPoles, pr = nPercentRipple, bType = bFilterType;
 pCoeffA = new double [(MAX_COEFFICIENTS+1)];
 pCoeffB = new double [(MAX_COEFFICIENTS+1)];
 pTA = new double [(MAX_COEFFICIENTS+1)];
 pTB = new double [(MAX_COEFFICIENTS+1)];
 memset(pCoeffA, 0, sizeof(double) * (MAX_COEFFICIENTS+1));
 memset(pCoeffB, 0, sizeof(double) * (MAX_COEFFICIENTS+1));
 memset(pTA, 0, sizeof(double) * (MAX_COEFFICIENTS+1));
 memset(pTB, 0, sizeof(double) * (MAX_COEFFICIENTS+1));
 pCoeffA[2] = 1.0;
 pCoeffB[2] = 1.0;
}

Chebyshev::~Chebyshev()
{
 delete [] pCoeffA;
 delete [] pCoeffB;
 delete [] pTA;
 delete [] pTB;
}

void Chebyshev::CalculateCoefficients()
{
 int i;
 for(int poleNum = 1; poleNum <= (np/2); poleNum++)
 {
     double A[3], B[3];
	 CalculateCascade(A, B, poleNum);
	 // Add coefficients to the cascade...
	 memcpy(pTA, pCoeffA, sizeof(double) * (MAX_COEFFICIENTS+1));
	 memcpy(pTB, pCoeffB, sizeof(double) * (MAX_COEFFICIENTS+1));
     for(i = 2; i <= MAX_COEFFICIENTS; i++)
	 {
         pCoeffA[i] = A[0]*pTA[i] + A[1]*pTA[i-1] + A[2]*pTA[i-2];
         pCoeffB[i] = pTB[i] - B[1]*pTB[i-1] - B[2]*pTB[i-2];
	 }
 }

 // Finish combining coefficients
 pCoeffB[2] = 0;
 for(i = 0; i <= (MAX_COEFFICIENTS - 2); i++)
 {
     pCoeffA[i] = pCoeffA[i+2];
     pCoeffB[i] = -pCoeffB[i+2];
 }

 // Normalize the gain
 double SA = 0;
 double SB = 0;
 for(i = 0; i <= (MAX_COEFFICIENTS - 2); i++)
 {
     if(bType == LOPASS)
     {
        SA += pCoeffA[i];
        SB += pCoeffB[i];
     }else
	 {
        if((i % 2) == 0)
		{
            SA +=  pCoeffA[i];
            SB +=  pCoeffB[i];
		}else
		{
            SA += (pCoeffA[i] * -1.0);
            SB += (pCoeffB[i] * -1.0);
		}
	 }
 }

 double GAIN = SA / (1.0 - SB);
 for(i = 0; i <= (MAX_COEFFICIENTS - 2); i++)
 {
     pCoeffA[i] /= GAIN;
 }

 // The final recursion coefficients are in pCoeffA[] and pCoeffB[]
}


// Call CalculateCascade() in a loop, like so...
/*
 double A[3], B[3];

 for(int poleNum = 1; poleNum <= (np/2); poleNum++)
 {
	 CalculateCascade(A, B, poleNum);
	 do something with the coefficients,
	 like save them or run the filter for the given stage
 }

*/

void Chebyshev::CalculateCascade(double *pA, double *pB, int poleNum)
{
 double tmp, RP, IP, ES, VX, KX, T, T_SQRD, W, M, D, K, K_SQRD, X0, X1, X2, Y1, Y2;

 // Calculate the pole location on the unit circle
 RP = -cos(PI/(np*2) + (poleNum-1) * PI/np);
 IP =  sin(PI/(np*2) + (poleNum-1) * PI/np);

 // Warp from a circle to an ellipse
 if(pr > 0)
 {
    tmp = (100.0 / (100.0-pr));
	tmp *= tmp;
    ES = sqrt(tmp - 1.0);
	tmp = 1.0 / (ES*ES);
    VX = (1.0/np) * log( (1.0/ES) + sqrt(tmp + 1.0) );
    KX = (1.0/np) * log( (1.0/ES) + sqrt(tmp - 1.0) );
    KX = (exp(KX) + exp(-KX))/2.0;
    RP = RP * ( (exp(VX) - exp(-VX) ) /2.0 ) / KX;
    IP = IP * ( (exp(VX) + exp(-VX) ) /2.0 ) / KX;
 }

 // s-domain to z-domain conversion
 T = 2 * tan(0.5);
 T_SQRD = T*T;
 W = 2.0*PI*fc;
 M = (RP*RP) + (IP*IP);
 D = 4.0 - 4.0*RP*T + M*T_SQRD;
 X0 = T_SQRD/D;
 X1 = 2.0*X0;
 X2 = X0;
 Y1 = (8.0 - 2*M*T_SQRD)/D;
 Y2 = (-4.0 - 4.0*RP*T - M*T_SQRD)/D;

 // LP TO LP, or LP TO HP transform
 tmp = W/2.0;
 if(bType == HIPASS)
 {
	K = -cos(tmp + 0.5) / cos(tmp - 0.5);
 }else
 {
    K =  sin(0.5 - tmp) / sin(0.5 + tmp);
 }
 K_SQRD = K*K;
 D = 1.0 + Y1*K - Y2*K_SQRD;
 pA[0] = (X0 - X1*K + X2*K_SQRD)/D;
 pA[1] = (-2.0*X0*K + X1 + X1*K_SQRD - 2.0*X2*K)/D;
 pA[2] = (X0*K_SQRD - X1*K + X2)/D;
 pB[0] = 0; 
 pB[1] = (2.0*K + Y1 + Y1*K_SQRD - 2.0*Y2*K)/D;
 pB[2] = (-K_SQRD - Y1*K + Y2)/D;
 if(bType == HIPASS)
 {
	pA[1] = -pA[1];
	pB[1] = -pB[1];
 }
}

// 08 April 2013 - Getting the coefficients for a cascade - 3 pairs per pole - I think
void Chebyshev::GetFilter(double *pA, double *pB)
{
 int i, j;

 for(i = 0; i <= np; i++)
 {
	 for(j = 0; j < 3; j++)
	 {
         *pA++ = pCoeffA[(i*3) + j];
         *pB++ = pCoeffB[(i*3) + j];
	 }
 }
}
