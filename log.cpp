#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdexcept>

#include "core.h"
#include "log.h"

// if the log has to be placed in a non-existing folder, that folder has to be created first, or the logging will fail
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

const char *CLog::GetLogFilename()
{
	return m_aLogFullPath;
}

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