#pragma once

#include <thread>

#define MAX_LEN_FILENAME 256
#define MAX_LEN_FILEPATH 256

#define FILEPATH_BASE "home/black/server"
#define FOLDERPATH_LOG FILEPATH_BASE "/log"
#define LOG_NAME "_log.txt"

#define DELAY_MAINLOOP 100000
#define DELAY_KEYBOARDCONTROLLOOP 100000

// enums
typedef enum
{
	THR_SERVERRUN,
	THR_KEYBOARDCONTROL,
	AMOUNT_THREADS,
} E_THREADS;

// structs
typedef struct
{
	char aFilepathLog[MAX_LEN_FILEPATH];
	std::thread aThread[AMOUNT_THREADS];
	int aThreadStatus[AMOUNT_THREADS];
	int exitApplication;
} S_MAIN;

// public variables
extern S_MAIN gsMain;