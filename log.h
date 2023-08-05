#pragma once

// value defines
#define CLOG_MSG_MAXLEN 1024
#define CLOG_PATH_MAXLEN 1024

// classes
class CLog
{
public:
	CLog(const char *pLogFilename);
	void Log(const char *pMessage, ...);

private:
	int WriteToFile(const char *pMessage);

	char m_aLogFilename[CLOG_PATH_MAXLEN];
};