#include <signal.h>
#include <unistd.h>

#include "server.h"
#include "log.h"
#include "core.h"
#include "main.h"

/* TODO
	GENERAL
		Constructor has to initialize all members to zero!
		Port to C++ and modularize completely
		split main and cmain (rename)
		Use common coding conventions (Enums, Namespaces, server::mosfet_clear belongs to hardware, etc...)
		strlen --> strnlen usw...
		Knowledge: c++ what if source is made into library but header file contains data that the source file depends on
		header die nicht gebraucht werden löschen
		username-independend paths
		Check if something can be made smarter... (core?)
		Clean up everything / Code formatter { 0 }
		Test every single function again

	GPIO
		set pinmode
		read pin
		set/read multiple pins at once

	MOSFET
		Implement function "mosread"

	INTERFACE
		Make it so you can connect with a browser and click on buttons instead of sending commands

	END
		Documentation please!
*/

// global variables
S_MAIN gsMain = {0};

// private functions
void *main_ThrfKeyboardcontrol(void *pArgs);
int main_ExitApplication();

int main()
{
	int retval = 0;
	struct sigaction sigAction = {0};
	int hasError = false;
	int i = 0;
	void *(*pFunc)(void *) = 0;

	// create log folder path
	retval = Mkdir(FOLDERPATH_LOG); // -------------- TODO will this work?
	if (retval != OK)
	{
		printf("%s: Failed to make folder \"%s\"\n", FOLDERPATH_LOG);
		return ERROR;
	}

	snprintf(gsMain.aFilepathLog, ARRAYSIZE(gsMain.aFilepathLog), "%s/%s", FOLDERPATH_LOG, LOG_NAME);

	// initialize log
	CLog cLog(gsMain.aFilepathLog);

	// make process ignore SIGPIPE signal, so it will not exit when writing to disconnected socket
	sigAction.sa_handler = SIG_IGN;
	retval = sigaction(SIGPIPE, &sigAction, NULL);
	if (retval != 0)
	{
		Log("%s: Failed to ignore the SIGPIPE signal", __FUNCTION__);
		return ERROR;
	}

	// run server and keybordcontrol thread
	CServer cServer(&cLog);

	for (i = 0; i < ARRAYSIZE(gsMain.aThread); ++i)
	{
		if (i == 0)
			pFunc = cServer.ThrfRun;
		else if (i == 1)
			pFunc = main_ThrfKeyboardcontrol;

		gsMain.aThreadStatus[i] = OK;
		retval = pthread_create(&gsMain.aThread[i], NULL, pFunc, NULL);
		if (retval != 0)
		{
			cLog.Log("%s: Failed to create thread %d", __FUNCTION__, i);
			return ERROR;
		}
	}

	// main loop
	while (1)
	{
		// check thread status
		for (i = 0; i < ARRAYSIZE(gsMain.aThread); ++i)
		{
			if (gsMain.aThreadStatus[i] != OK)
			{
				cLog.Log("%s: There was an error in thread %d, ending...", __FUNCTION__, i);
				hasError = true;
				break;
			}
		}

		// check if application should exit
		if (hasError || gsMain.exitApplication)
		{
			cLog.Log("Exiting application...");

			retval = main_ExitApplication();
			if (retval != OK)
				return ERROR;

			break;

			break;
		}

		usleep(DELAY_MAINLOOP);
	}

	cLog.Log("Application end");

	if (hasError)
		return ERROR;

	return OK;
}

void *main_ThrfKeyboardcontrol(void *pArgs)
{
	char ch = 0;

	while (1)
	{
		// keyboard input
		ch = getchar();

		if (ch == 'e')
			gsMain.exitApplication = true;

		usleep(DELAY_KEYBOARDCONTROLLOOP);
	}

	return (void *)OK;
}

int main_ExitApplication()
{
	int i = 0;
	int retval = 0;
	pthread_t thrServerExitApp = 0;
	int hasError = false;

	// server on exit application
	retval = pthread_create(&thrServerExitApp, NULL, server_ThrfOnExitApplication, NULL);
	if (retval != 0)
	{
		cLog.Log("%s: Failed to create thread for exiting server applications", __FUNCTION__, i);
		hasError = true;
	}

	// join thread and wait for finish
	pthread_join(thrServerExitApp, (void **)&retval);
	if (retval != OK)
		hasError = true;

	// cancel main threads
	for (i = 0; i < ARRAYSIZE(gsMain.aThread); ++i)
	{
		retval = pthread_cancel(gsMain.aThread[i]);
		if (retval != 0)
		{
			cLog.Log("%s: There was an error cancelling thread %d", __FUNCTION__, i);
			hasError = true;
		}
	}

	if (hasError)
		return ERROR;

	return OK;
}