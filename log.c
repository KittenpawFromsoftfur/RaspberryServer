#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "core.h"
#include "log.h"

S_LOGDATA gsLogdata = { 0 };

// private functions
int log_WriteToFile(const char *pMessage);
int log_CheckFilenameFormat(const char *pFilename);

// if the log has to be placed in a non-existing folder, that folder has to be created first, or the logging will fail
int log_Init(const char *pLogFilename)
{
	int retval = 0;
	struct stat sDir = { 0 };

	memset((void*)&gsLogdata, 0, sizeof(gsLogdata));
	strncpy(gsLogdata.aLogFilename, pLogFilename, ARRAYSIZE(gsLogdata.aLogFilename));
	gsLogdata.isInitialized = 1;

	return OK;
}

void Log(const char *pMessage, ...)
{
	char buffer[LOG_MSG_MAXLEN] = { 0 };
    va_list argptr;

	if (!gsLogdata.isInitialized)
	{
		printf("%s: Failed to log, log is not initialized\n", __FUNCTION__);
		return;
	}

    va_start(argptr, pMessage);
	vsprintf(buffer, pMessage, argptr);
	printf("%s\n", buffer);
	log_WriteToFile(buffer);
    va_end(argptr);
}

int log_WriteToFile(const char *pMessage)
{
	FILE *pFile = 0;
	time_t t = time(0);
	struct tm sTime = *localtime(&t);

	pFile = fopen(gsLogdata.aLogFilename, "a");
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