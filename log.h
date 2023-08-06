#pragma once

// value defines
#define CLOG_MSG_MAXLEN 1024
#define CLOG_PATH_MAXLEN 1024

// classes
class CLog
{
public:
	CLog(const char *pFolder, const char *pLogFile);
	void Log(const char *pMessage, ...);
	const char *GetLogFilename();

private:
	int WriteToFile(const char *pMessage);

	char m_aLogFullPath[CLOG_PATH_MAXLEN];
};