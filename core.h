#ifndef CORE_H
#define CORE_H

#include <stdio.h>
#include <time.h>

// value defines
#define ERROR -1
#define OK 0
#define true 1
#define false 0

#define _CORE_MAXLEN_STRINGIGNORE 256
#define _CORE_MAXLEN_FILEPATH 256

// function defines
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))

#define STRINGIFY_NAME(a) #a
#define STRINGIFY_VALUE(a) STRINGIFY_NAME(a)

#define TIME_TO_SECONDS_HMS(h, m, s) (60 * 60 * h + 60 * m + s)

// public functions
int core_StringCompareNocase(const char *pSource, const char *pDest, size_t Len);
void core_StringCopyIgnore(char *pDest, const char *pSource, size_t Len, const char *pIgnore);
void core_StrRemove(char *pSource, size_t Len, const char *pRem);
void core_StringToUpper(char *pSource, size_t Len);
void core_StringToLower(char *pSource, size_t Len);
int core_Mkdir(const char *pPath);
int core_CheckFileExists(const char *pDirname, const char *pFilename, size_t LenFilename);
int core_CountFilesDirectory(const char *pDirname);
int core_CheckStringAscii(const char *pString, size_t Len);
int core_RemoveFile(const char *pFilename);
int core_RemoveFilesDirectory(const char *pDirname);
int core_IsLetter(char Char);
int core_MeasureStart();
double core_MeasureStop(int Print);

#endif