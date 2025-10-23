//
// Created by User on 5/12/2020.
//

#include <ctime>
#include <cstring>
#include <cmath>
#include <android/log.h>
#include "LogFile.h"


// Expects file was open for READ
void LogFile::Rewind()
{
    if(fpLog == nullptr)return;
    fseek(fpLog, 0, SEEK_SET);
}

void LogFile::ExportOnRead(FILE *fp)
{
    fpExport = fp;
    bCopyOnExport = true;
}


// VER <int value> file version
//  Writes something like... VER 100
// First version is 100, will increment 101, 102, 103, etc.
void LogFile::writeVersion(int nVersion)
{
    if(fpLog == nullptr)return;
    char numstr[16];
    memset(numstr, 0, 16);
    sprintf(numstr, "VER %d\n", nVersion);
    fputs(numstr, fpLog);
}

// Only called when file first opened
// (and therefore we expect file pointer is pointing at version)
// returns -1 for error
int LogFile::readVersion()
{
    if(fpLog == nullptr)return -1;
    if(feof(fpLog))return -1;
    int nVersion = 0;
    char numstr[16];
    memset(numstr, 0, 16);
    fgets (numstr, 16, fpLog);
    // Put a null terminator after the 3 letter identifier
    numstr[3] = '\0';
    if(strcmp(numstr, "VER")) // Returns 0 for 'VER', thus no error and no early exit
    {
        // strcmp returns 1 for anything but 'VER'
        return -1;
    }
    // value begins at &numstr[4]
    nVersion = atoi(&numstr[4]);
    return nVersion;
}

void LogFile::WriteTriggerDB(float dB)
{
    if(fpLog == nullptr)return;
    char outstr[64];
    memset(outstr, 0, 64);
    sprintf(outstr, "SET trigger dB: %f\n", dB);
    fputs(outstr, fpLog);
}
void LogFile::WriteMinTriggerFrequency(float fFreq)
{
    if(fpLog == nullptr)return;
    char outstr[64];
    memset(outstr, 0, 64);
    sprintf(outstr, "SET trigger min freq: %f\n", fFreq);
    fputs(outstr, fpLog);
}
void LogFile::WriteMaxTriggerFrequency(float fFreq)
{
    if(fpLog == nullptr)return;
    char outstr[64];
    memset(outstr, 0, 64);
    sprintf(outstr, "SET trigger max freq: %f\n", fFreq);
    fputs(outstr, fpLog);
}

// BPR <string><space delimiter> <int value> BuffersInPreroll
// Writes something like... BPR 5
// Only called after opening file or
// when it is expected after the end of a group
void LogFile::writeBuffersInPreroll(int nBuffersInPreroll)
{
    if(fpLog == nullptr)return;
    fputs("BPR ", fpLog);
    char val = static_cast<char>('0' + nBuffersInPreroll);
    fputc(val, fpLog);
    fputc('\n', fpLog);
}

// Only called when it is expected that
// file pointer is pointing at BuffersInPreroll
// Otherwise detected in ReadNextItem()
// returns -1 for error
int LogFile::readBuffersInPreroll()
{
    if(fpLog == nullptr)return - 1;
    if(feof(fpLog))return -1;
    int nBuffersInPreroll = 0;
    char numstr[64]; // was 16
    memset(numstr, 0, 64);  // was 16
    fgets (numstr, 64, fpLog);  // was 16
    // Put a null terminator after the 3 letter identifier
    numstr[3] = '\0';
    while(!strcmp(numstr, "SET"))
    {
        if(bCopyOnExport)
        {
            numstr[3] = ' ';
            fputs(numstr, fpExport);
        }
        memset(numstr, 0, 64);
        fgets(numstr, 64, fpLog);
        // Put a null terminator after the 3 letter identifier
        numstr[3] = '\0';
    }

    if(strcmp(numstr, "BPR"))  // Returns 0 for 'BPR', thus no error and no early exit
    {
        return -1;
    }
    // value begins at &numstr[4]
    nBuffersInPreroll = atoi(&numstr[4]);
    return nBuffersInPreroll;
}

/*
    EVT <string><space delimiter> <int value> event time,  file offset count samples
    Followed by: <string>

    Writes something like...
    EVT 11:19:57 356352
    Peak frequency: 459.21 Hz, dB: -47.3
*/
void LogFile::writeEvent(char *pTriggerDetails, int nFileOffsetInSamples)
{
    if(fpLog == nullptr)return;
    char numstr[32];
    memset(numstr, 0, 32);
    strcpy(numstr, "EVT ");
    OutPutTimeString(&numstr[4]);
    int len = strlen(numstr);
    sprintf(&numstr[len], " %d\n", nFileOffsetInSamples);
    fputs(numstr, fpLog);
    fputs(pTriggerDetails, fpLog);
    fputc('\n', fpLog);
}

// NVT <no value> indicate a non-event.
// EVs & NVs may be intermingled,
// but 4 NVs in a row indicates end of current events group
void LogFile::writeNonevent()
{
    if(fpLog == nullptr)return;
    fputs("NVT\n", fpLog);
}

/*
    EOG <int value><space delimiter><int value> count events in group, count callbacks in group

    Writes something like...
    EOG 9 22
 */
void LogFile::writeEndOfGroup(int nEventsInGroup, int nCallbacks)
{
    if(fpLog == nullptr)return;
    char numstr[32];
    memset(numstr, 0, 32);
    sprintf(numstr, "EOG %d %d\n", nEventsInGroup, nCallbacks);
    fputs(numstr, fpLog);
}

/*
   EOF <int value> <space> <int val>  - indicates end of file

   Writes something like...
   EOF 123 1196
*/
void LogFile::writeEndOfFile(int nTotalEventsWrittenToLog, int nCountContiguousSamplesGroups)
{
    if(fpLog == nullptr)return;
    char numstr[16];
    memset(numstr, 0, 16);
    sprintf(numstr, "EOF %d %d\n", nTotalEventsWrittenToLog, nCountContiguousSamplesGroups);
    fputs(numstr, fpLog);
}

// Next item could be any of: BPR, EVT, NVT, EOG, EOF, ERR, SET
// Expects pStr1 is TIMEBUFSIZE, pStr2 is TEXTBUFSIZE
FileItems LogFile::ReadNextItem(char *pStr1, char *pStr2, int *pVal1, int *pVal2)
{
    if(fpLog == nullptr)return ERR;
    if(feof(fpLog))return ERR;
    // char info[64];
    char numstr[64];
    char *ptr, *pEnd;
    *pVal1 = 0; *pVal2 = 0;
    memset(numstr, 0, 64);
    fgets(numstr, 64, fpLog);
    // Put a null terminator after the 3 letter identifier
    numstr[3] = '\0';
    while(!strcmp(numstr, "SET"))
    {
        if(bCopyOnExport)
        {
            numstr[3] = ' ';
            fputs(numstr, fpExport);
        }
       memset(numstr, 0, 64);
       fgets(numstr, 64, fpLog);
       // Put a null terminator after the 3 letter identifier
       numstr[3] = '\0';
    }
    if(!strcmp(numstr, "BPR"))
    {
        *pVal1 = atoi(&numstr[4]);
        return BPR;
    }else if(!strcmp(numstr, "EVT"))
    {
       // numstr contains EVT 11:19:57 356352 with a 0 after EVT instead of the space char it had
       numstr[12] = '\0'; // Put a null terminator after the time
       strcpy(pStr1, &numstr[4]); // Return the time stamp
       *pVal1 = atoi(&numstr[13]); // Return count samples
       memset(pStr2, 0, TEXTBUFSIZE);
       fgets(pStr2, TEXTBUFSIZE, fpLog); // Get TriggerDetails "Peak frequency: 459.21 Hz, dB: -47.3", which we will ignore

       if(bCopyOnExport)
       {
           fputs("Event time: ", fpExport);
           fputs(pStr1, fpExport);
           fputc('\n', fpExport);
           fputs("Sample offset: ", fpExport);
           fputs(&numstr[13], fpExport);
           fputs(pStr2, fpExport);
           fputc('\n', fpExport);
       }

       return EVT;
    }else if(!strcmp(numstr, "NVT"))
    {
       // No info to return
       return NVT;
    }else if(!strcmp(numstr, "EOG"))
    {
       // numstr contains EOG 9 22 with a 0 after EOG instead of the space char it had
        pEnd = ptr = &numstr[4]; // points to "9 22" or whatever number of digits each had
        int len = strlen(ptr);
        int count = 0; // avoid endless loop if error
        while(*(++pEnd) != ' ')
        {
            ++count;
            if(count == len)break;
        }
        // Assume pEnd is now positioned at end of count events
        if(count < len)
        {
            *pEnd = '\0';
            *pVal1 = atoi(ptr);
            ptr = ++pEnd; // now pointing to count callbacks
            *pVal2 = atoi(ptr);
        }
       return EOG;
    }else if(!strcmp(numstr, "EOF"))
    {
        // numstr contains EOF 123 1196 with a 0 after EOF instead of the space char it had
        pEnd = ptr = &numstr[4]; // points to "123 1196" or whatever number of digits each had
        int len = strlen(ptr);
        int count = 0; // avoid endless loop if error
        while(*(++pEnd) != ' ')
        {
            ++count;
            if(count == len)break;
        }
        // Assume pEnd is now positioned at end of nTotalEventsWrittenToLog
        if(count < len)
        {
            *pEnd = '\0';
            *pVal1 = atoi(ptr);
            ptr = ++pEnd; // now pointing to count nCountContiguousSamplesGroups
            *pVal2 = atoi(ptr);
        }
       return FEOF;
    } else
    {
        if(bCopyOnExport)
        {
            fputs("There was some error reading log!", fpExport);
        }
       return ERR;
    }
}
