#ifndef GRAPH_H
 #define GRAPH_H


#include "FFTManager.h"


#define OCTAVES 10
#define FIRST_OCTAVE_A 27.5  // freq. of the note 'A' from the beginning octave we are using
#define NOTES_PER_OCTAVE 12
#define COUNT_NOTES (OCTAVES * NOTES_PER_OCTAVE)

#define EIGHTS_TONE_MUSICAL_NOTES (OCTAVES * 48) // notes for octaves beginning @ C0 16.35 hz to C9 8372 hz
#define BAND_LIMITS (OCTAVES * 96) // demarcations for musical notes' upper and lower boundaries

#define FRAMEDGRAPH_WIDTH  482
#define FRAMEDGRAPH_HEIGHT 322
#define GRAPH_WIDTH (EIGHTS_TONE_MUSICAL_NOTES)
#define GRAPH_HEIGHT 320

// The last 3 notes in 10 octaves
// are of a frequency higher than the FFT can deliver...
#define DISPLAYABLE_MUSICAL_NOTES (EIGHTS_TONE_MUSICAL_NOTES - 3)
#define LAST_DISPLAYABLE_MUSICAL_NOTE_INDEX (DISPLAYABLE_MUSICAL_NOTES - 1)
#define INDEX_C6 288 // (6 x 48)

typedef struct _RECT
{
 int left;
 int right;
 int top;
 int bottom;
}RECT, *LPRECT;

typedef struct _LINEDATA
{
 uint32_t *pMainBmp;
 int x;
 int y;
 int imageWidth;
 int height;
 uint32_t colour;
}LINEDATA, *LPLINEDATA;

const uint32_t WHITE = 0x00FFFFFF;


static char *Notes[] = { 
 (char *)"C0",
 (char *)"C#0/Db",
 (char *)"D0",
 (char *)"D#0/Eb",
 (char *)"E0",
 (char *)"F0",
 (char *)"F#0/Gb",
 (char *)"G0",
 (char *)"G#0/Ab",
 (char *)"A0",
 (char *)"A#0/Bb",
 (char *)"B0",
//--------
 (char *)"C1",
 (char *)"C#1/Db",
 (char *)"D1",
 (char *)"D#1/Eb",
 (char *)"E1",
 (char *)"F1",
 (char *)"F#1/Gb",
 (char *)"G1",
 (char *)"G#1/Ab",
 (char *)"A1",
 (char *)"A#1/Bb",
 (char *)"B1",
//--------
 (char *)"C2",
 (char *)"C#2/Db",
 (char *)"D2",
 (char *)"D#2/Eb",
 (char *)"E2",
 (char *)"F2",
 (char *)"F#2/Gb",
 (char *)"G2",
 (char *)"G#2/Ab",
 (char *)"A2",
 (char *)"A#2/Bb",
 (char *)"B2",
//--------
 (char *)"C3",
 (char *)"C#3/Db",
 (char *)"D3",
 (char *)"D#3/Eb",
 (char *)"E3",
 (char *)"F3",
 (char *)"F#3/Gb",
 (char *)"G3",
 (char *)"G#3/Ab",
 (char *)"A3",
 (char *)"A#3/Bb",
 (char *)"B3",
//--------
 (char *)"C4",
 (char *)"C#4/Db",
 (char *)"D4",
 (char *)"D#4/Eb",
 (char *)"E4",
 (char *)"F4",
 (char *)"F#4/Gb",
 (char *)"G4",
 (char *)"G#4/Ab",
 (char *)"A4",
 (char *)"A#4/Bb",
 (char *)"B4",
//--------
 (char *)"C5",
 (char *)"C#5/Db",
 (char *)"D5",
 (char *)"D#5/Eb",
 (char *)"E5",
 (char *)"F5",
 (char *)"F#5/Gb",
 (char *)"G5",
 (char *)"G#5/Ab",
 (char *)"A5",
 (char *)"A#5/Bb",
 (char *)"B5",
//--------
 (char *)"C6",
 (char *)"C#6/Db",
 (char *)"D6",
 (char *)"D#6/Eb",
 (char *)"E6",
 (char *)"F6",
 (char *)"F#6/Gb",
 (char *)"G6",
 (char *)"G#6/Ab",
 (char *)"A6",
 (char *)"A#6/Bb",
 (char *)"B6",
//--------
 (char *)"C7",
 (char *)"C#7/Db",
 (char *)"D7",
 (char *)"D#7/Eb",
 (char *)"E7",
 (char *)"F7",
 (char *)"F#7/Gb",
 (char *)"G7",
 (char *)"G#7/Ab",
 (char *)"A7",
 (char *)"A#7/Bb",
 (char *)"B7",
//--------
 (char *)"C8",
 (char *)"C#8/Db",
 (char *)"D8",
 (char *)"D#8/Eb",
 (char *)"E8",
 (char *)"F8",
 (char *)"F#8/Gb",
 (char *)"G8",
 (char *)"G#8/Ab",
 (char *)"A8",
 (char *)"A#8/Bb",
 (char *)"B8",
//--------
 (char *)"C9",
 (char *)"C#9/Db",
 (char *)"D9",
 (char *)"D#9/Eb",
 (char *)"E9",
 (char *)"F9",
 (char *)"F#9/Gb",
 (char *)"G9",
 (char *)"G#9/Ab",
 (char *)"A9",
 (char *)"A#9/Bb",
 (char *)"B9"
};


class MusicalSpectrum
{
public:
 MusicalSpectrum();
 ~MusicalSpectrum();
 void ResetGraph(void);
 float GetMusicalNote(int x);
 static bool isMusical(float fReferenceFreq, float fTestFreq);
 bool isMusical(int index, float fTestFreq);
 char *GetMusicalNoteTextVal(float fFrequency);
 int FrequencyToMusicalNoteArrayOffset(float frequency);
 void DoGraph(uint32_t *pMainBmp, int imageWidth, int imageHeight, bool bRecording);
 char *FindPeaks(FFTManager* pFFTManager, float fMinFreq, float fMaxFreq);

 float fPeakFreq;
 float fPeakDBs;

private:
 static void GenerateOctave(float A, float *pScale);
 static void GenerateEvenTemperedScale(float A, int numOctaves, float **pArray);
 void GenerateEighthToneScale(void);
 static void GenerateEighthToneOctave(float A, float *fScale);
 void GenerateSixteenthToneScale(void);
 static void GenerateSixteenthToneOctave(float A, float *fScale);
 static int BinarySearch(const float *pArray, int arrayLength, float fVal);
 static int GetIndexNearestNeighbour(float *pArray, int arrayLength, float fVal);
 void ZeroGraphData(void);
 void GenerateSinePalette(bool bSaturated, bool bRgb);
 static int CalculateColour(int countColours, int index, bool bSaturated, bool bRgb);
 static u_int16_t RGBto565Colour(u_int8_t red, u_int8_t green, u_int8_t blue);
 static u_int16_t Get565Colour(u_int16_t red, u_int16_t green, u_int16_t blue);
 static u_int32_t RGBto565Doubled(uint32_t rgb);
 static void VerticalLineDown(LPLINEDATA pLineData);
 void DrawFrame(uint32_t *pMainBmp, int imageWidth, LPRECT pFrameRect);
 float *pNoteArray; // [COUNT_NOTES];
 float *EightsToneMusicalScale{}; // [EIGHTS_TONE_MUSICAL_NOTES];
 float *pfBandLimits{}; // [BAND_LIMITS];
 float *BandPeakDBs; // [DISPLAYABLE_MUSICAL_NOTES];

 float fMinimumFreq;
 RECT framedGraphRect{};
 RECT GraphRect{};
 uint32_t WhiteDoubled;
 uint32_t *pColours; // [EIGHTS_TONE_MUSICAL_NOTES];
 int *pGraphData; // [EIGHTS_TONE_MUSICAL_NOTES];
 char *pPeakMusicalNote{};
};


#endif

