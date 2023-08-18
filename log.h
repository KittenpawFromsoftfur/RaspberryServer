/**
 * @file log.h
 * @author KittenpawFromsoftfur (finbox.entertainment@gmail.com)
 * @brief Simple logging such as printing on the console and simultaneously writing a log to the file system
 * @version 1.0
 * @date 2023-08-18
 *
 * @copyright Copyright (c) 2023
 *
 */
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