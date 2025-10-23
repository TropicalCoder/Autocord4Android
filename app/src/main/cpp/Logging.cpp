//
// Created by User on 6/1/2020.
//
#include <ctime>
#include <cstring>
#include <cmath>
#include <android/log.h>
#include "Logging.h"

bool Logging::OpenForWrite(char *pPathname)
{
    fpLog = fopen(pPathname, "wt");
    return (fpLog != nullptr);
}

bool Logging::OpenForRead(char *pPathname)
{
    fpExport = nullptr;
    bCopyOnExport = false;

    fpLog = fopen(pPathname, "rt");
    return (fpLog != nullptr);
}

// In one test of a file of 441 bytes, this returns 429,
// so it is not reliable for anything
int Logging::CloseFile()
{
    int len = 0;
    if(bCopyOnExport)
    {
        fclose(fpExport);
        fpExport = nullptr;
        bCopyOnExport = false;
    }
    if(fpLog != nullptr)
    {
        len = ftell(fpLog);
        fclose(fpLog);
        fpLog = nullptr;
    }
    return len;
}

// Assumes file was opened for read
int Logging::GetFileLength()
{
    if(fpLog == nullptr)return 0;
    fseek(fpLog, 0, SEEK_END);
    int len = ftell(fpLog);
    fseek(fpLog, 0, SEEK_SET);
    return len;
}

void Logging::OutputDateString()
{
    char DateString[64];
    struct tm *today;
    time_t ltime;

    time(&ltime); // Gets time in seconds since UTC 1/1/70
    today = localtime( &ltime ); // Convert to time structure

    memset(DateString, 0, 64);

    // Use strftime to build a customized date string...
    strftime(DateString, 64, "%A, %B/%d/%Y ", today);

    fputs(DateString, fpLog);
}


// If pTextBuf != NULL, copy time stamp
// to buffer passed instead of output to file
void Logging::OutPutTimeString(char *pTextBuf)
{
    char TimeStr[16];
    char numstr[8];
    struct tm *today;
    time_t ltime;

    time(&ltime); // Gets time in seconds since UTC 1/1/70
    today = localtime( &ltime ); // Convert to time structure

    // Build the string...
    memset(TimeStr, 0, 16);

    // hours number 0 - 23 (hours since midnight)
    if(today->tm_hour == 0)
    {
        TimeStr[0] = '0';
        TimeStr[1] = '0';
    }else
    {
        if(today->tm_hour < 10)
        {
            TimeStr[0] = '0';
            // TimeStr[1] = '\0';
        }
        sprintf(numstr, "%d", today->tm_hour);
        // itoa(today->tm_hour, numstr, 10);
        strcat(TimeStr, numstr);
    }
    TimeStr[2] = ':';
    // TimeStr[3] = '\0';

    // minutes number 0 - 59
    if(today->tm_min == 0)
    {
        TimeStr[3] = '0';
        TimeStr[4] = '0';
    }else
    {
        if(today->tm_min < 10)
        {
            TimeStr[3] = '0';
            // TimeStr[4] = '\0';
        }
        // itoa(today->tm_min, numstr, 10);
        sprintf(numstr, "%d", today->tm_min);
        strcat(TimeStr, numstr);
    }
    TimeStr[5] = ':';
    // TimeStr[6] = '\0';

    // seconds number 0 - 59
    if(today->tm_sec == 0)
    {
        TimeStr[6] = '0';
        TimeStr[7] = '0';
    }else
    {
        if(today->tm_sec < 10)
        {
            TimeStr[6] = '0';
            // TimeStr[7] = '\0';
        }
        // itoa(today->tm_sec, numstr, 10);
        sprintf(numstr, "%d", today->tm_sec);
        strcat(TimeStr, numstr);
    }

    if(pTextBuf != nullptr)
    {
        TimeStr[8] = '\0';
        strcpy(pTextBuf, TimeStr);
        return;
    }

    TimeStr[8] = '\n';
    TimeStr[9] = '\0';
    fputs(TimeStr, fpLog);
}

// Assumes file was opened for read
// Assumes bufferLength <= fileLength
// (ie: Assumes GetFileLength() was called before this call)
int Logging::ReadFileToBuffer(char *pBuffer, int bufferLength)
{
    if(fpLog == nullptr)return 0;
    int lenRead = fread(pBuffer, 1, (size_t)bufferLength, fpLog);
    fseek(fpLog, 0, SEEK_SET);
    return lenRead;
}
