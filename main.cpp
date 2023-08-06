#include <signal.h>
#include <unistd.h>

#include "server.h"
#include "log.h"
#include "core.h"
#include "main.h"

/* TODO
	GENERAL
		Port to C++ and modularize completely
		split main and cmain (rename)
		CServer structs in klassen splitten und in Funktionen aufteilen
		set/mosset --> set mosfet/gpio ; dasselbe mit allen kommandos...
>		username-independend paths
		Check if something can be made smarter... (core?)
		Thrf... --> Normale Fnktion?
		Clean up everything / Sort functions properly / Code formatter { 0 }
		main() with args so Port can be set from command line etc...
		Server has many functions with pointers to own members --> pass only slotindex

		ENDEND
			Each class has to initialize members to 0 in constructor
			threads have to be detached

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
int main_ThrfKeyboardcontrol();
int main_ExitApplication(CServer *pServer, CLog *pLog);

int main()
{
	int retval = 0;
	struct sigaction sigAction = {0};
	int hasError = false;
	int i = 0;
	int (*pFunc)() = 0;

	// create log folder path
	retval = CCore::Mkdir(FOLDERPATH_LOG);
	if (retval != OK)
	{
		printf("%s: Failed to make folder \"%s\"\n", FOLDERPATH_LOG);
		return ERROR;
	}

	snprintf(gsMain.aFilepathLog, ARRAYSIZE(gsMain.aFilepathLog), "%s/%s", FOLDERPATH_LOG, LOG_NAME);

	// initialize log
	CLog Log(gsMain.aFilepathLog);

	// initialize server
	CServer Server(&gsMain, &Log);

	// make process ignore SIGPIPE signal, so it will not exit when writing to disconnected socket
	sigAction.sa_handler = SIG_IGN;
	retval = sigaction(SIGPIPE, &sigAction, NULL);
	if (retval != 0)
	{
		Log.Log("%s: Failed to ignore the SIGPIPE signal", __FUNCTION__);
		return ERROR;
	}

	// run server thread
	gsMain.aThreadStatus[THR_SERVERRUN] = OK;
	gsMain.aThread[THR_SERVERRUN] = std::thread(&CServer::ThrfRun, &Server);

	// start keyboard control thread
	gsMain.aThreadStatus[THR_KEYBOARDCONTROL] = OK;
	gsMain.aThread[THR_KEYBOARDCONTROL] = std::thread(&main_ThrfKeyboardcontrol);

	// main loop
	while (1)
	{
		// check thread status
		for (i = 0; i < ARRAYSIZE(gsMain.aThread); ++i)
		{
			if (gsMain.aThreadStatus[i] != OK)
			{
				Log.Log("%s: There was an error in thread %d, ending...", __FUNCTION__, i);
				hasError = true;
				break;
			}
		}

		// check if application should exit
		if (hasError || gsMain.exitApplication)
		{
			Log.Log("Exiting application...");

			retval = main_ExitApplication(&Server, &Log);
			if (retval != OK)
				return ERROR;

			break;

			break;
		}

		usleep(DELAY_MAINLOOP);
	}

	Log.Log("Application end");

	if (hasError)
		return ERROR;

	return OK;
}

int main_ThrfKeyboardcontrol()
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

	return OK;
}

int main_ExitApplication(CServer *pServer, CLog *pLog)
{
	int i = 0;
	int retval = 0;
	int hasError = false;

	// exit server
	retval = pServer->OnExitApplication();
	if (retval != OK)
		hasError = true;

	// cancel main threads
	for (i = 0; i < ARRAYSIZE(gsMain.aThread); ++i)
	{
		retval = CCore::DetachThreadSafely(&gsMain.aThread[i]);
		if (retval != OK)
		{
			pLog->Log("%s: Failed to detach thread %d", __FUNCTION__, i);
			hasError = true;
		}
	}

	if (hasError)
		return ERROR;

	return OK;
}