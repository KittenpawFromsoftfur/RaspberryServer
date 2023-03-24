#ifndef LOG_H
#define LOG_H

// value defines
#define LOG_MSG_MAXLEN 1024
#define LOG_PATH_MAXLEN 1024

// structs
typedef struct
{
	char aLogFilename[LOG_PATH_MAXLEN];
	int isInitialized;
}S_LOGDATA;

// public functions
int log_Init(const char *pLogFilename);
void Log(const char *pMessage, ...);

#endif