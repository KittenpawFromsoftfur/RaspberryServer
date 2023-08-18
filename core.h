/**
 * @file core.h
 * @author KittenpawFromsoftfur (finbox.entertainment@gmail.com)
 * @brief Core functions for any program
 * @version 1.0
 * @date 2023-08-18
 *
 * @copyright Copyright (c) 2023
 */
#pragma once

#include <stdio.h>
#include <time.h>
#include <thread>

// value defines
#define ERROR -1
#define OK 0

#define CCORE_MAXLEN_STRINGIGNORE 256
#define CCORE_MAXLEN_FILEPATH 256

// function defines
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))

#define STRINGIFY_NAME(a) #a
#define STRINGIFY_VALUE(a) STRINGIFY_NAME(a)

#define TIME_TO_SECONDS_HMS(h, m, s) (60 * 60 * h + 60 * m + s)

// classes
class CCore
{
public:
    CCore();
    static int Mkdir(const char *pPath);
    static int RemoveFile(const char *pFilename);
    static int RemoveFilesDirectory(const char *pDirname);
    static int CountFilesDirectory(const char *pDirname);
    static int CheckFileExists(const char *pDirname, const char *pFilename, size_t LenFilename);
    static int StringCompareNocase(const char *pSource, const char *pDest, size_t Len);
    static void StringCopyIgnore(char *pDest, const char *pSource, size_t Len, const char *pIgnore);
    static void StringRemove(char *pSource, size_t Len, const char *pRem);
    static void StringToUpper(char *pSource, size_t Len);
    static void StringToLower(char *pSource, size_t Len);
    static bool CheckStringAscii(const char *pString, size_t Len);
    static bool IsLetter(char Char);
    static bool IsNumber(char Char);
    static int DetachThreadSafely(std::thread *pThread);
    int MeasureStart();
    double MeasureStop(bool Print);

private:
    struct timespec m_sTimeStart;
};