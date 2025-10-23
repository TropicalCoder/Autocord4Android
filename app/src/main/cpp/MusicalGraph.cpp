#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <android/log.h>
#include "MusicalGraph.h"


MusicalSpectrum::MusicalSpectrum()
{
 pNoteArray = new float[COUNT_NOTES];
 GenerateEvenTemperedScale(FIRST_OCTAVE_A, OCTAVES, &pNoteArray);
 GenerateEighthToneScale(); // creates & fills EightsToneMusicalScale
 GenerateSixteenthToneScale(); // creates & fills pfBandLimits

 BandPeakDBs = new float[DISPLAYABLE_MUSICAL_NOTES];
 fPeakFreq = 0;
 fPeakDBs = 0;

 // Get the frequency one half step below the lowest note C0;
 fMinimumFreq = (float)(27.5 * pow(2, (-73.0 / 96.0)) );
 // Get the frequency one half step above the highest note B9;
 // fMaximumFreq = (float)(14080.0 * pow(2, (17.0 / 96.0)) ); // 15918.7898639
 // note value at DISPLAYABLE_MUSICAL_NOTES: 16034.143 
 // note value at LAST_DISPLAYABLE_MUSICAL_NOTE_INDEX: 15804.26564 (B9)
 
 // All these arrays are 'EIGHTS_TONE_MUSICAL_NOTES' in size,
 // but we only use them up to 'DISPLAYABLE_MUSICAL_NOTES'
 pGraphData = new int [EIGHTS_TONE_MUSICAL_NOTES];
 pColours = new uint32_t [EIGHTS_TONE_MUSICAL_NOTES];
 memset(&GraphRect, 0, sizeof(RECT));
 WhiteDoubled = RGBto565Doubled(WHITE);
 GenerateSinePalette(false, true);
 ZeroGraphData(); // depends on GraphRect
}


MusicalSpectrum::~MusicalSpectrum()
{
 // 20200113 added test for null before delete
 // 05062020 - learned that here in Android deleting NULL pointer has no effect
 delete [] pNoteArray;
 delete [] pfBandLimits;
 delete [] EightsToneMusicalScale;
 delete [] BandPeakDBs;
/*
 delete [] OctaveNotes;
 delete [] OctavePeakFreqs;
 delete [] OctavePeakDBs;
*/
 delete [] pGraphData;
 delete [] pColours;
 fPeakFreq = 0;
 fPeakDBs = 0;
}

// Given A for the octave,
// generate the 12 notes of the octave from C thru B
void MusicalSpectrum::GenerateOctave(float A, float *pScale)
{
    // double twelfths;

    for (int i = -9; i <= 2; i++)
    {
        if (i == 0)
        {
            pScale[i + 9] = A;
            continue;
        }

        // twelfths = (double)i / 12.0;
        pScale[i + 9] = (A * (float)pow(2, ((double)i / 12.0)));
    }
}

// Given the frequency of an 'A' for the first octave,
// generate all the notes for numOctaves requested...
// (A0 = 27.5, A1 = 55, A2 = 110, etc.)
void MusicalSpectrum::GenerateEvenTemperedScale(float A, int numOctaves, float **pArray)
{
    float *pScale = *pArray;
    float n = A; // A0 = 27.5
    for (int i = 0; i < numOctaves; i++)
    {
        GenerateOctave(n, &pScale[i * NOTES_PER_OCTAVE]);
        n *= 2;
    }
}

void MusicalSpectrum::GenerateEighthToneScale()
{
 float n = 27.5; // A0

 EightsToneMusicalScale = new float [EIGHTS_TONE_MUSICAL_NOTES];
 if(EightsToneMusicalScale == nullptr)return;

 for(int i = 0; i < OCTAVES; i++)
 {
     GenerateEighthToneOctave(n, &EightsToneMusicalScale[i * 48]);
     n *= 2;
 }
}

void MusicalSpectrum::GenerateEighthToneOctave(float A, float *fScale)
{
 double fourtyeighths;

 for(int i = -36; i <= 11; i++)
 {
     if(i == 0)
     {
        fScale[i+36] = A;
        continue;
     } 

     fourtyeighths = (double)i / 48.0;
     fScale[i+36] = (float)((double)A * pow(2, fourtyeighths) );
 }
}

// We create an array filled with "band limits" that demark
// the boundary between "eighth tones" frequencies
void MusicalSpectrum::GenerateSixteenthToneScale()
{
 float n = 27.5; // A0

 pfBandLimits = new float [BAND_LIMITS];
 if(pfBandLimits == nullptr)return;

 for(int i = 0; i < OCTAVES; i++)
 {
     GenerateSixteenthToneOctave(n, &pfBandLimits[i * 96]);
     n *= 2;
 }
}

void MusicalSpectrum::GenerateSixteenthToneOctave(float A, float *fScale)
{
 double ninetysixths;

 for(int i = -72; i <= 23; i++)
 {
     if(i == 0)
     {
        fScale[i+72] = A;
        continue;
     } 

     ninetysixths = (double)i / 96.0;
     fScale[i+72] = (float)((double)A * pow(2, ninetysixths) );
 }
}

// Pixels are imageWidth x imageHeight 16 bit values,
// but we will be drawing a graph with lines 2 pixels wide
void MusicalSpectrum::DoGraph(uint32_t *pMainBmp, int imageWidth, int imageHeight, bool bRecording)
{
 // Scale the height of the graph according to the height of the bitmap
 float scaleHeight, fHeight;

 int yOffset = 48;
 if(imageWidth < imageHeight)
 {
    // Phone is vertical, take half the height
    scaleHeight = (float)(imageHeight / 2) / FRAMEDGRAPH_HEIGHT;
    fHeight = (float)FRAMEDGRAPH_HEIGHT * scaleHeight;
 }else
 {
     // Phone is horizontal, take 3/4 the height
     scaleHeight = ((float)imageHeight * 3 / 4.f) / FRAMEDGRAPH_HEIGHT;
     fHeight = (float)FRAMEDGRAPH_HEIGHT * scaleHeight;
     yOffset /= 2;
 }


 imageWidth /= 2; // because we are writing 2 pixels at a time, but counting them as 1 pixel width

 int xOffset = imageWidth / 2 - FRAMEDGRAPH_WIDTH / 2;

 framedGraphRect.left = xOffset;
 framedGraphRect.top = yOffset;
 framedGraphRect.right = framedGraphRect.left + FRAMEDGRAPH_WIDTH;
 framedGraphRect.bottom = framedGraphRect.top + (int)fHeight;

 // GraphRect is the black area within framedGraphRect
 GraphRect.top = framedGraphRect.top + 1;
 GraphRect.left = framedGraphRect.left + 1;
 GraphRect.right = GraphRect.left + GRAPH_WIDTH;
 GraphRect.bottom = GraphRect.top + (int)fHeight - 2;

 float dBscale = (fHeight - 2) / 96.33f; // height / maxdB

 DrawFrame(pMainBmp, imageWidth, &framedGraphRect);


 // Each x is 2 pixels but each y is only 1 pixel
 // Function draw from x or y up to and including right or bottom
 // (imageWidth / 2) is centre of screen
 // yOffset is top line of box
 // We want a square maybe 40 x 40,
 // and since x are double that means 20 double pixels wide
 // and 40 pixels high
 framedGraphRect.left = (imageWidth / 2) - 20; // minus 5 double pixels
 framedGraphRect.top = yOffset -42;
 framedGraphRect.right = framedGraphRect.left + 20; // 10 double pixels wide (20 pixels)
 framedGraphRect.bottom = framedGraphRect.top + 40;

 LINEDATA LineData;
 LineData.pMainBmp = pMainBmp;
 LineData.imageWidth = imageWidth;

 int *pData = pGraphData; // UPDATE 300 - trying to understand this
 // Lines are drawn from peak at 'y' down to graph bottom
 // pGraphData is just an array the width of the display
 // and I save the 'y' for each line drawn.
 // Currently I do nothing further with that information.
 // Maybe at the time I though I would clear the screen by redrawing those lines
 // in black, but it appears I am just doing a blit to clear the screen.
 // Oh I see, I think I save the line for next draw, 
 // so I don't have to redraw the entire line - just the difference from last time. Cool!
 // graph area is 480 x 320
 // dB values after negation range from 0 to 96.3
 // After MULT * -dB range from 0 to 385 - interesting, 'cause graph height is only 320
 // but back when I developed this I erroneously thought max dB to be 90.3, x 4 = 361
 // still higher than the graph! However, I do clip the top and bottom to fit on screen
 // and I don't care to show very low signal levels which are cut off at the bottom which is .
 // which is (320 / 4 = 80) 80 dB down - 05/06/2020 no loner true
 // What I am trying to understand at the moment is that when I normalize the amplitudes
 // of the FFT results, why don't I see peaks at the top of the graph?
 // Instead the highest peaks are 20 to 30 dB down from the top.
 // Before we get here peaks are converted to negative dB and clipped to be between 0 & -MaxDBs (-96.3294661...)
 // Of course we can plainly see the value at peak frequency which is always displayed.
 // Yet where FFT results are normalized in FFTManager before scanning for peaks at least one bin
 // gets a value of 1.0 in amplitude when divided by max
 // but the moment I looked peak was found at bin 1, and I don't even begin scanning for peaks until bin 17
 // Ah I know what it is... If I have a peak somewhere that matters, it does show as normalized - at the top,
 // but when there is no peak within the range I scan for the graph drops down again.
 // In other words, with normalization I would have expected to see at least one peak touching the top of the graph,
 // but the displayed peaks are not normalized to highest displayed peak,
 // only the bin values are normalized. If max peak is not displayed, it will appear there was no normalization.


 // So we see here
 // y = GraphRect.top + (int)(dBscale * -BandPeakDBs[x]); // multiply the dB value by dBscale (height / maxdB)
 // if value is 0 dB it will be drawn full height on the graph, from top to bottom

 // Using MoveTo/LineTo:      Average microseconds per run: 569, num runs: 13274
 // Using line drawing funcs: Average microseconds per run: 228, num runs: 13277
 int x, y, dif, countErased = 0;
 uint32_t *pLineOffset = pMainBmp + ((GraphRect.bottom - 1) * imageWidth + GraphRect.left);
 for(x = 0; x < DISPLAYABLE_MUSICAL_NOTES; x++)
 {
     // I currently have a bug where I will see a black "ghost" peak in black in the last few octaves,
     // with the spectral data riding above it.
     // My guess is that somehow the bitmap got erased without graph data having been reset.
     // This code believes there is always a vertical coloured line on the bitmap representing the previous peak,
     // and extends or shortens that line. If in fact there is no such line, we will have a ghost spectrum.
     // Why I only see it towards the end of the graph is because at some point every line is completely redrawn
     // at lower frequencies as there may be no peak at moments in a given band. However, at higher frequencies
     // there is always some peak to be found because the bandwidth is exponentially wider as we go up the graph.
     // We could just refresh the entire graph ever half second and that would solve the problem.
     // Obviously the better way to deal with this is to find the cause. Currently testing, and it seems
     // if you leave the screen off long enough, it must go black, because when I turn it back on I see
     // the ghost and this code in action. But I can't replicate such behavior on demand.
     // At the moment I'm guessing that the screen compositor must
     // blank the screen and stop calling canvass Redraw after awhile
     // but it could be bad data in the audio file I am playing... but I can't imagine what form that would take.

	 if(BandPeakDBs[x] < 0)  // UPDATE 300 Was if(BandPeakDBs[x] < 0), shouldn't it be if(BandPeakDBs[x] <= 0) ? When I do that graph is a mess.
     {                       // Oh I see BandPeakDBs[] is initialized to 0, so if no peak in a given slot, its value is 0
                             // Instead it should be initialize to some positive value like 1
                             // except I see where I gather the peak values in BandPeakDBs[] I check for 0 to see if we have a peak for that band yet.
                             // Solution is in case dB value is found to be 0 (no attenuation), it should be given a very tiny negative value
                             // Ok - did that in FFT::ParabolicInterpolation
                             // if (peak >= 1.0) peak = -0.000001;
                             // but it made no difference that I could see. With normalization the highest peak was still down maybe 20 dB
        y = GraphRect.top + (int)(dBscale * -BandPeakDBs[x]); // multiply the dB value by scale factor
/*       We don't need this check
        if(y < GraphRect.top)
        {
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "y < GraphRect.top y: %d", y);
           y = GraphRect.top;
        }else if(y > GraphRect.bottom)
        {
            __android_log_print(ANDROID_LOG_DEBUG, __func__, "y > GraphRect.bottom y: %d", y);
           y = GraphRect.bottom;
        }
*/
     }else y = GraphRect.bottom;

     dif = (y - *pData);

     if(dif > 0)
	 {
        // Erase part of the existing line to reduce its height
        LineData.x = x + GraphRect.left;
        LineData.y = *pData;
        LineData.height = dif; // y - *pData;
        LineData.colour = 0;
        VerticalLineDown(&LineData);
	 }else if(dif < 0) // (y < *pData)
	 {
        // Extend existing line upwards to increase its height
        LineData.x = x + GraphRect.left;
        LineData.y = y;
        LineData.height = -dif; // *pData - y;
        LineData.colour = pColours[x];
        VerticalLineDown(&LineData);
     }

     *pData = y;
     // Sometimes the bitmap erases itself for unknown reason
     // Will have a ghost that appears to be black but tests show pixel is not 0
     // so can't test for 0. We don't know what it is, but at least
     // we can test to see if it is the colour it is supposed to be.
     // Look to see if the line - that is supposed to be coloured - has a problem
     if((GraphRect.bottom - *pData) >= 2)
     {
         if(*pLineOffset != pColours[x])
         {
             ++countErased;
             // Forcing the line to be redrawn
             if(y >= GraphRect.top)
             {
                 LineData.x = x + GraphRect.left;
                 LineData.y = y;
                 LineData.height = (GraphRect.bottom - y);
                 LineData.colour = pColours[x];
                 VerticalLineDown(&LineData);
             }
         }
     }
/*
     else if(y == GraphRect.bottom)
     {
         if(*pLineOffset != 0)
         {
            ++countNotErased;
         }
     }
*/
     ++pLineOffset;
	 ++pData;
 }
 //   __android_log_print(ANDROID_LOG_DEBUG, __func__, "Graph countExtended: %d, countLowered: %d, countNotErased: %d, countColourSame: %d, countColourDifferent: %d, countLinesExtended: %d, countLinesLowered: %d", countExtended, countLowered, countNotErased, countColourSame, countColourDifferent, countLinesExtended, countLinesLowered);
#ifdef DEBUG_LOGGING
 if(countErased)
    __android_log_print(ANDROID_LOG_DEBUG, __func__, "Graph countErased: %d", countErased);
#endif
}

float MusicalSpectrum::GetMusicalNote(int x)
{
 if(EightsToneMusicalScale == nullptr)
 {
     return 16.f;
 }
 if(x <= 0)
 {
    return 16.f;
 }
 if(x > LAST_DISPLAYABLE_MUSICAL_NOTE_INDEX)
 {
    return 16000.f;
 }
 return EightsToneMusicalScale[x];
}

// Return the index of element < freq
int MusicalSpectrum::BinarySearch(const float *pArray, int arrayLength, float fVal)
{
    int left = 0;
    int right = arrayLength;

    while (left < right)
    {
        int m = (left + right) / 2;
        if (pArray[m] > fVal)
        {
            right = m;
        }else
        {
            left = m + 1;
        }
    }

    --right;
    return right;
}

int MusicalSpectrum::GetIndexNearestNeighbour(float *pArray, int arrayLength, float fVal)
{
    int index = BinarySearch(pArray, arrayLength, fVal);

    if (index < 0)
    {
        // Freq < pNoteArray[0]
        return 0;
    }

    if (index == (arrayLength - 1))
    {
        // Freq >= pNoteArray[COUNT_NOTES - 1]
        return arrayLength - 1;
    }

    // ((freq >= pNoteArray[index]) && (freq < pNoteArray[index + 1]))
    float difA = fVal - pArray[index];
    float difB = pArray[index + 1] - fVal;

    if (difA <= difB)
    {
        return index;
    }

    return index + 1;
}

int MusicalSpectrum::FrequencyToMusicalNoteArrayOffset(float frequency)
{
    if(frequency <= 16.f)
    {
        return 0;
    }else if(frequency >= 16000.f)
    {
        return LAST_DISPLAYABLE_MUSICAL_NOTE_INDEX;
    }

    return GetIndexNearestNeighbour(EightsToneMusicalScale, DISPLAYABLE_MUSICAL_NOTES, frequency);
}

// For some reason, this works under the debugger or for a debug version of the app
// but not for a release version!
bool MusicalSpectrum::isMusical(float fReferenceFreq, float fTestFreq)
{
    auto fUpperLimit = (float)(fReferenceFreq * pow(2.0, 0.125 / 12.0));
    auto fLowerLimit = (float)(fReferenceFreq * pow(2.0, -0.125 / 12.0));
    return ((fTestFreq > fLowerLimit) && (fTestFreq < fUpperLimit));
}

// I think this is more correct anyhow. Tested.
// For 440 Hz, this algorithm in col1, above algorithm in col2
//               col1         col2
// fUpperLimit = 443.270477  443.188416
// fLowerLimit = 436.913086  436.834503
// This algorithm was tested for a note 15% above and below 440 Hz, ie
// fUpperLimit = 443.924561
// fLowerLimit = 436.295715
// and verified it will not pass as A4 ie - returns false
bool MusicalSpectrum::isMusical(int index, float fTestFreq)
{
    float fNextDif = pNoteArray[index + 1] - pNoteArray[index];
    float fPrevDif = pNoteArray[index] - pNoteArray[index - 1];
    float fUpperLimit = pNoteArray[index] + fNextDif * 0.125f;
    float fLowerLimit = pNoteArray[index] - fPrevDif * 0.125f;
    return ((fTestFreq > fLowerLimit) && (fTestFreq < fUpperLimit));
}

char *MusicalSpectrum::GetMusicalNoteTextVal(float fFrequency)
{
    int index = GetIndexNearestNeighbour(pNoteArray, COUNT_NOTES, fFrequency);
    if (isMusical(index, fFrequency))
    {
        // if one knows the frequencies a and b of two notes,
        // the number of cents measuring the interval from a to b
        // may be calculated by the following formula (similar to the definition of a decibel)
        // float centsDifferent = 1200.f * log2f(fFrequency / pNoteArray[index]);
        return Notes[index];
    }
    return nullptr;
}

// Assumes we did  dB = pFFTManager->GetPeakFreq(pSamples, &fPeakFrequency)
// just before calling this func.

// When fTriggerFreqMin: 16.000000, fTriggerFreqMax: 16000.000000
// loBin = 8, hiBin = 8191

// pFFTManager->GetBinsForFrequencyRange(16.f, 16000.f, int* pLoBin, int* pHiBin)
char *MusicalSpectrum::FindPeaks(FFTManager* pFFTManager, float fMinFreq, float fMaxFreq)
{
 float fTmpPeakFreq;
 float fTmpPeakDBs;
 float fMaxDBs = -pFFTManager->GetMaxDB();

 int i, j, k; //, octave; // curBand = -1;
 int minBin, maxBin;
 pFFTManager->GetBinsForFrequencyRange(16.f, 16000.f, &minBin, &maxBin);
 memset(BandPeakDBs, 0, sizeof(float) * DISPLAYABLE_MUSICAL_NOTES);

 fPeakFreq = 0;
 fPeakDBs = fMaxDBs;
 pPeakMusicalNote = nullptr;



// Was this...
// int loBin, hiBin;
// pFFTManager->GetBinsForFrequencyRange(fMinFreq, fMaxFreq, &loBin, &hiBin);
// for(k = loBin; k < hiBin; k++)

// I used to only scan from loBin to hiBin with frequency triggering
// Then the graph would only display frequencies in that range, and remain black on either side
// I thought that was cool to only show potentially triggering frequencies, and it remained
// that way for a long time until now - Oct 14, 2020
// Now I don't think it looks cool anymore, because we are blind to what is going on in the spectrum
// outside the band of frequencies between fMinFreq and fMaxFreq, and it just looks broken
// So now I display the full spectrum. The code in AudioEngine->HandleRecord() simply checks
// bTriggered = ((dB > fTriggerDBs) && (fPeakFrequency >= fTriggerFreqMin) && (fPeakFrequency <= fTriggerFreqMax));
// and doesn't care what the spectrum is displaying.
// Only, without some kind of adjustment, we will display peak frequency and musical note outside the range
// of triggering, when these happen to be outside the range.
// Furthermore, when setting the trigger, it will react to frequencies outside the range.
// So we need to ensure that we accumulate these statistics only when within the range
// Added this test down below, where we are getting peak frequency & it's dB value...

// Get highest peak of all
// ...
// if( (fTmpPeakFreq >= fMinFreq) && (fTmpPeakFreq <= fMaxFreq) )  // new 20201014

 i = 0;
 for(k = minBin; k < maxBin; k++)
 {
     fTmpPeakDBs = pFFTManager->GetPeakFreq(k, &fTmpPeakFreq); // returns 0 in fTmpPeakFreq if no peak;

	 if(fTmpPeakFreq < 1)
     { 
		continue;
     }
	 if(fTmpPeakDBs <= fMaxDBs)
     {
		// Found a peak, but it is too low to bother with
	    ++k; // skip next bin
	    continue;
     }

	 // We have a peak! Where is it?

	 if(!i)
     {
        // We don't increment i until a frequencies >= pfBandLimits[1] arrives
        if(fTmpPeakFreq < fMinimumFreq)
        {
		   ++k; // skip next bin
		   continue;
        }

        if(fTmpPeakFreq < pfBandLimits[1])
        {
 	       if((BandPeakDBs[0] == 0) || (fTmpPeakDBs > BandPeakDBs[0]))
           {
               BandPeakDBs[0] = fTmpPeakDBs;

               if( (fTmpPeakFreq >= fMinFreq) && (fTmpPeakFreq <= fMaxFreq) ) // new 20201014
               {
                   fPeakDBs = fTmpPeakDBs;
                   fPeakFreq = fTmpPeakFreq;
                   pPeakMusicalNote = Notes[0];
               }
		   }
	       ++k; // skip next bin
		   continue;
		}else i = 2;

     }
	 

	 // We have a peak! Where is it?

//	 if(fTmpPeakFreq < pfBandLimits[i-1])
//   {
//      Doesn't happen - well tested
//		continue;
//   }
     // UPDATE 300 - here finding the slot for fTmpPeakFreq
     // where pfBandLimits[i] < fTmpPeakFreq < pfBandLimits[i+1]
     // We can increment by i+=2 because no need to test both sides of above equation
	 while((fTmpPeakFreq >= pfBandLimits[i+1]) && (i < BAND_LIMITS))i+=2;

	 if(i >= BAND_LIMITS)break;

//	 if((fTmpPeakFreq < pfBandLimits[i-1])  || (fTmpPeakFreq >= pfBandLimits[i+1]))
//   {
//      Doesn't happen - well tested
//	 }	  

     j = i/2;

     // 20200114 Discovered that j can exceed size of BandPeakDBs[DISPLAYABLE_MUSICAL_NOTES] and thereby overrun array boundary
     if (j >= DISPLAYABLE_MUSICAL_NOTES)
     {
         break; // ...so added this
     }
     // We can have more than one peak frequency for a given band.
     // We want the highest peak for this current band.
	 if((BandPeakDBs[j] == 0) || (fTmpPeakDBs > BandPeakDBs[j]))
     {
         BandPeakDBs[j] = fTmpPeakDBs;

         // Get highest peak of all - for given frequency range
         if(fTmpPeakDBs > fPeakDBs)
         {
             if( (fTmpPeakFreq >= fMinFreq) && (fTmpPeakFreq <= fMaxFreq) ) // new 20201014
             {
                 fPeakDBs = fTmpPeakDBs;
                 fPeakFreq = fTmpPeakFreq;
                 if ((j % 4) == 0)
                 {
                     pPeakMusicalNote = Notes[j / 4];
                 } else
                 {
                     pPeakMusicalNote = nullptr;
                 }
             }
         }
     } // if((BandPeakDBs[j] == 0) || (fTmpPeakDBs > BandPeakDBs[j]))

     // Now we go on to look for the next peak
	 // However, it is impossible for next bin to be a peak
	 // since we just proved that the bin before has a greater amplitude...
	 ++k; // skip next bin
 }
 return pPeakMusicalNote;
}


void MusicalSpectrum::ResetGraph()
{
// SelectObject(hdcSpare, hBmpFramedGraph);
// BitBlt(hdcScreen, framedGraphRect.left, framedGraphRect.top, FRAMEDGRAPH_WIDTH, FRAMEDGRAPH_HEIGHT, hdcSpare, 0, 0, SRCCOPY);

 // fPrevDBs = 0;
 ZeroGraphData();
}


void MusicalSpectrum::ZeroGraphData()
{
 if(pGraphData == nullptr)return;
 for(int i = 0; i < EIGHTS_TONE_MUSICAL_NOTES; i++)
 {
     pGraphData[i] = GraphRect.bottom;
 }
}


void MusicalSpectrum::VerticalLineDown(LPLINEDATA pLineData)
{
    uint32_t *pStart = pLineData->pMainBmp + (pLineData->y * pLineData->imageWidth) + pLineData->x;
    uint32_t *pEnd = pStart + (pLineData->height * pLineData->imageWidth);

    while (pStart < pEnd)
    {
        *pStart = pLineData->colour;
        pStart += pLineData->imageWidth;
    }
}

void MusicalSpectrum::DrawFrame(uint32_t *pMainBmp, int imageWidth, LPRECT pFrameRect)
{
    auto *pTop = (pMainBmp + (pFrameRect->top * imageWidth) + pFrameRect->left);
    auto *pTop2 = (pMainBmp + ((pFrameRect->top + 1) * imageWidth) + pFrameRect->left);
    auto *pBottom = (pMainBmp + (pFrameRect->bottom * imageWidth) + pFrameRect->left);
    auto *pBottom2 = (pMainBmp + ((pFrameRect->bottom + 1) * imageWidth) + pFrameRect->left);

    uint32_t *pLeft = pTop;
    auto *pRight = (pMainBmp + (pFrameRect->top * imageWidth) + pFrameRect->right);

    for(int x = 0; x < (pFrameRect->right - pFrameRect->left); x++)
    {
        *pTop++ = WhiteDoubled;
        *pTop2++ = WhiteDoubled;
        *pBottom++ = WhiteDoubled;
        *pBottom2++ = WhiteDoubled;
    }

    for(int y = 0; y < (1 + pFrameRect->bottom - pFrameRect->top); y++)
    {
        *pLeft = WhiteDoubled;
        *pRight = WhiteDoubled;
        pLeft += imageWidth;
        pRight += imageWidth;
    }
}


u_int16_t MusicalSpectrum::RGBto565Colour(u_int8_t red, u_int8_t green, u_int8_t blue)
{
    auto r = (u_int16_t)((float)red * 0x1F / 255.f);
    auto g = (u_int16_t)((float)green * 0x3F / 255.f);
    auto b = (u_int16_t)((float)blue * 0x1F / 255.f);
    return Get565Colour(r, g, b);
}

u_int16_t MusicalSpectrum::Get565Colour(u_int16_t red, u_int16_t green, u_int16_t blue)
{
    u_int16_t mask5 = 0x1F; // 31
    u_int16_t mask6 = 0x3F; // 63
    u_int16_t colour = ((red & mask5) << 11) | ((green & mask6) << 5) | (blue & mask5);

    return colour;
}

// Assumes hi byte (alpha channel) of value passed is 0
uint32_t MusicalSpectrum::RGBto565Doubled(uint32_t rgb)
{
    u_int16_t colour16 = RGBto565Colour((u_int8_t)(rgb & 0x000000FF), (u_int8_t) ((rgb >> 8) & 0x000000FF), (u_int8_t) ((rgb >> 16) & 0x000000FF));
    u_int32_t colour32 = (colour16 << 16) | colour16;

    return colour32;
}

// Generates a weird palette which, when non-saturated looks sort of iridescent like a soap bubble film
// Pass bRgb = true for use with my graphics routines
int MusicalSpectrum::CalculateColour(int countColours, int index, bool bSaturated, bool bRgb)
{
    // const double PI = 3.1415926535897932384626433832795;
    const double TWO_PI = 6.283185307179586476925286766559;
    const double ONE_THIRD_TWO_PI = 2.0943951023931954923084289221863;
    const double TWO_THIRD_TWO_PI = 4.1887902047863909846168578443727;

    // With bSaturated = true or false, we try to start out with most red as first palette entry for RGB
    // countColours += 200;
    // countColours %= EIGHTS_TONE_MUSICAL_NOTES;

    double dFrac = 0; // Shifts the spectrum left or right

    if (bSaturated)
    {
        dFrac = (TWO_PI * 197 / 360.0);
        // A palette of 360 colours starts at FA 28 59 and ends at FA 26 5B
    }
    else
    {
        dFrac = (TWO_PI * 210 / 360.0);
        // A palette of 360 colours starts at FF 7F 7F and ends at FE 81 7D
    }

    double dBluePhaseAngle = fmod((dFrac + (double)index * TWO_PI / countColours), TWO_PI);
    double dGreenPhaseAngle = fmod((dBluePhaseAngle + ONE_THIRD_TWO_PI), TWO_PI);
    double dRedPhaseAngle = fmod((dBluePhaseAngle + TWO_THIRD_TWO_PI), TWO_PI);

    int dwBlue, dwGreen, dwRed;
    if (bSaturated)
    {
        // Pure sine waves
        dwBlue = 127 + (int)floor(127.0 * sin(dBluePhaseAngle));
        dwGreen = 127 + (int)floor(127.0 * sin(dGreenPhaseAngle));
        dwRed = 127 + (int)floor(127.0 * sin(dRedPhaseAngle));
    }
    else
    {
        // Full wave rectified...
        dwBlue = (int)floor(255 * fabs(sin(dBluePhaseAngle)));
        dwGreen = (int)floor(255 * fabs(sin(dGreenPhaseAngle)));
        dwRed = (int)floor(255 * fabs(sin(dRedPhaseAngle)));
    }


    if (bRgb)
    {
        // Return as RGB colour...
        return ((dwRed << 16) | (dwGreen << 8) | dwBlue);
    }
    else
    {
        // Return as BGR colour...
        return ((dwBlue << 16) | (dwGreen << 8) | dwRed);
    }
}

// Generates mMaxColours colours - same as Palette array length
void MusicalSpectrum::GenerateSinePalette(bool bSaturated, bool bRgb)
{
    int countColours = 0;
    if (bSaturated)
    {
        for (; countColours < EIGHTS_TONE_MUSICAL_NOTES; countColours++)
        {
            pColours[countColours] = RGBto565Doubled(
                    static_cast<uint32_t>(CalculateColour(EIGHTS_TONE_MUSICAL_NOTES, countColours, bSaturated,
                                                          bRgb)));
        }
    }
    else
    {
        for (; countColours < EIGHTS_TONE_MUSICAL_NOTES; countColours++)
        {
            // You have to ask for twice as many colours as you are going to use if passing bSaturated = false
            pColours[countColours] = RGBto565Doubled(
                    static_cast<uint32_t>(CalculateColour(EIGHTS_TONE_MUSICAL_NOTES * 2, countColours,
                                                          bSaturated, bRgb)));
        }
    }

    // Set the alpha...
    // for(int i = 0; i < mMaxColours; i++)
    // {
    //    mPalette[i] = mPalette[i] | 0xFF000000;
    // }

}
