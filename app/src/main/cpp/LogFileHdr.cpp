//
// Created by User on 5/12/2020.
//

#include <ctime>
#include <cstring>
#include <cmath>
#include <android/log.h>
#include "LogFileHdr.h"

void LogFileHdr::writeFileHeader(float fTriggerFreqMin, float fTriggerFreqMax, float fTriggerDBs)
{
    if(fpLog == nullptr)return;
    fputs("Date: ", fpLog);
    OutputDateString();
    fputs("- Time started: ", fpLog);
    OutPutTimeString(nullptr);
    writeTriggerParams(fTriggerFreqMin, fTriggerFreqMax, fTriggerDBs);
    fputs("\n", fpLog);
}

void LogFileHdr::writeTriggerParams(float fTriggerFreqMin, float fTriggerFreqMax, float fTriggerDBs)
{
    char TextBuf[128];
    memset(TextBuf, 0, 128);

    strcpy(TextBuf, "Min Trigger frequency: ");
    int len = strlen(TextBuf);
    if(fTriggerFreqMin < 10000.f)
    {
        snprintf(&TextBuf[len], 100, "%5.2f\n", fTriggerFreqMin);
    }else
    {
        snprintf(&TextBuf[len], 100, "%5.1f\n", fTriggerFreqMin);
    }

    len = strlen(&TextBuf[0]);
    strcpy(&TextBuf[len], "Max Trigger frequency: ");
    len = strlen(&TextBuf[0]);

    if(fTriggerFreqMax < 10000.f)
    {
        snprintf(&TextBuf[len], static_cast<size_t>(128 - len), "%5.2f\n", fTriggerFreqMax);
    }else
    {
        snprintf(&TextBuf[len], static_cast<size_t>(128 - len), "%5.1f\n", fTriggerFreqMax);
    }
    len = strlen(&TextBuf[0]);
    strcpy(&TextBuf[len], "Trigger dB: ");
    len = strlen(&TextBuf[0]);

    if(fTriggerDBs < 10.f)
    {
        snprintf(&TextBuf[len], static_cast<size_t>(128 - len), "%1.1f\n\n", fTriggerDBs);
    } else
    {
        snprintf(&TextBuf[len], static_cast<size_t>(128 - len), "%2.1f\n\n", fTriggerDBs);
    }
    fputs(TextBuf, fpLog);
}
