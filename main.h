#pragma once

#include <pthread.h>

#define MAX_LEN_FILENAME 256
#define MAX_LEN_FILEPATH 256

#define FOLDERPATH_LOG "/home/black/server/log"
#define LOG_NAME "_log.txt"

#define DELAY_MAINLOOP 100000
#define DELAY_KEYBOARDCONTROLLOOP 100000

// enums
typedef enum
{
	THR_SERVERRUN,
	THR_KEYBOARDCONTROL,
	AMOUNT_THREADS,
}E_THREADS;

// structs
typedef struct
{
	char aFilepathLog[MAX_LEN_FILEPATH];
	pthread_t aThread[AMOUNT_THREADS];
	int aThreadStatus[AMOUNT_THREADS];
	int exitApplication;
}S_MAIN;

// public variables
extern S_MAIN gsMain;