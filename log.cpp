/**
 * @file log.cpp
 * @author KittenpawFromsoftfur (finbox.entertainment@gmail.com)
 * @brief Simple logging such as printing on the console and simultaneously writing a log to the file system
 * @version 1.0
 * @date 2023-08-18
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdexcept>

#include "core.h"
#include "log.h"

/**
 * @brief	Construct a new CLog::CLog object
 *			If the log folder does not exist, logging will fail, so make sure to create it beforehand
 *
 * @param pFolder	Log folder name to place the log in or append to
 * @param pFile 	Log file name to be created or appended to
 */
CLog::CLog(const char *pFolder, const char *pFile)
{
	int retval = 0;

	memset(m_aLogFullPath, 0, ARRAYSIZE(m_aLogFullPath));

	// combine folder and file names
	snprintf(m_aLogFullPath, ARRAYSIZE(m_aLogFullPath), "%s/%s", pFolder, pFile);

	// make directory
	retval = CCore::Mkdir(pFolder);
	if (retval != OK)
	{
		printf("%s: Failed to make directory \"%s\"\n", __FUNCTION__, pFolder);
		throw std::runtime_error("CLog constructor failed due to inability to create the log directory!");
	}
}

/**
 * @brief Logs a message in the printf() format
 *
 * @param pMessage	Message to print
 * @param ... 		Format parameters
 */
void CLog::Log(const char *pMessage, ...)
{
	char buffer[CLOG_MSG_MAXLEN] = {0};
	va_list argptr;

	va_start(argptr, pMessage);
	vsprintf(buffer, pMessage, argptr);
	printf("%s\n", buffer);
	WriteToFile(buffer);
	va_end(argptr);
}

/**
 * @brief Returns the log file path consisting of folder and log name
 *
 * @return Log file path
 */
const char *CLog::GetLogFilename()
{
	return m_aLogFullPath;
}

/**
 * @brief Writes a message to a file on the file system
 *
 * @param pMessage Message to write
 *
 * @return OK
 * @return ERROR
 */
int CLog::WriteToFile(const char *pMessage)
{
	FILE *pFile = 0;
	time_t t = time(0);
	struct tm sTime = *localtime(&t);

	pFile = fopen(m_aLogFullPath, "a");
	if (pFile == 0)
	{
		printf("%s: Failed to open file\n", __FUNCTION__);
		return ERROR;
	}

	fprintf(pFile, "[%04d.%02d.%02d %02d:%02d:%02d]: ", sTime.tm_year + 1900, sTime.tm_mon + 1, sTime.tm_mday, sTime.tm_hour, sTime.tm_min, sTime.tm_sec);
	fprintf(pFile, "%s\n", pMessage);
	fclose(pFile);

	return OK;
}