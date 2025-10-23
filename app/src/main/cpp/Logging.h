//
// Created by User on 6/1/2020.
//

#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdlib.h>


class Logging
{
public:
    bool OpenForWrite(char *pPathname);
    bool OpenForRead(char *pPathname);
    int CloseFile();
    int GetFileLength();
    int ReadFileToBuffer(char *pBuffer, int bufferLength);
    void OutputDateString();
    void OutPutTimeString(char *pTextBuf);
protected:
    FILE *fpLog = NULL;
    FILE *fpExport = nullptr;
    bool bCopyOnExport = false;
};


#endif
