//
// Created by User on 5/12/2020.
//

#ifndef LOGFILEHDR_H
#define LOGFILEHDR_H

#include <stdio.h>
#include <stdlib.h>
#include "Logging.h"

class LogFileHdr : public Logging
{
public:
    // bool OpenForWrite(char *pPathname){OpenForWrite(pPathname);};
    void writeFileHeader(float fTriggerFreqMin, float fTriggerFreqMax, float fTriggerDBs);

private:
    void writeTriggerParams(float fTriggerFreqMin, float fTriggerFreqMax, float fTriggerDBs);
};


#endif
