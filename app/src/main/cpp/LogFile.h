//
// Created by User on 5/12/2020.
//

#ifndef LOGFILE_H
#define LOGFILE_H

#include <stdio.h>
#include <stdlib.h>
#include "Logging.h"

static const int TEXTBUFSIZE = 128;
static const int TIMEBUFSIZE = 16;

#define CURRENT_LOG_VERSION 100
enum FileItems{BPR=0, EVT=1, NVT=2, EOG=3, FEOF=4, ERR=5}; // Can't use 'EOF' because it is defined in stdio.h

class LogFile : public Logging
{
public:
    void Rewind(void);
    void ExportOnRead(FILE *fp);
    void writeVersion(int nVersion);
    int readVersion(void);
    void WriteTriggerDB(float dB);
    void WriteMinTriggerFrequency(float fFreq);
    void WriteMaxTriggerFrequency(float fFreq);
    void writeBuffersInPreroll(int nBuffersInPreroll);
    int readBuffersInPreroll(void);
    void writeEvent(char *pTriggerDetails, int nFileOffsetInSamples);
    void writeNonevent();
    void writeEndOfGroup(int nEventsInGroup, int nCallbacks);
    void writeEndOfFile(int nTotalEventsWrittenToLog, int nCountContiguousSamplesGroups);
    FileItems ReadNextItem(char *pStr1, char *pStr2, int *pVal1, int *pVal2);
};


#endif
