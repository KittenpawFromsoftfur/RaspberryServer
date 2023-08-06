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
		Clean up everything / Sort functions properly / Code formatter { 0 }
		main() with args so Port can be set from command line etc...
		Server has many functions with pointers to own members --> pass only slotindex

		ENDEND
			Each class has to initialize members to 0 in constructor
			threads have to be detached
			Sort functions

		Test every single function again

	GPIO
		set pinmode
		read pin
		set/read multiple pins at once

	MOSFET
		Implement function "mosread"
		More dynamic setting 0xFF

	INTERFACE
		Make it so you can connect with a browser and click on buttons instead of sending commands

	END
		Documentation please!
*/

// private functions
int main_Keyboardcontrol(S_MAIN *psMain);
int main_ExitApplication(S_MAIN *psMain, CServer *pServer, CLog *pLog);

int main()
{
	int retval = 0;
	int hasError = false;
	int i = 0;
	int (*pFunc)() = 0;
	S_MAIN sMain = {0};

	// create log folder path
	retval = CCore::Mkdir(FOLDERPATH_LOG);
	if (retval != OK)
	{
		printf("%s: Failed to make folder \"%s\"\n", FOLDERPATH_LOG);
		return ERROR;
	}

	snprintf(sMain.aFilepathLog, ARRAYSIZE(sMain.aFilepathLog), "%s/%s", FOLDERPATH_LOG, LOG_NAME);

	// initialize log
	CLog Log(sMain.aFilepathLog);

	// initialize server
	CServer Server(&sMain, &Log);

	// run server thread
	sMain.aThreadStatus[THR_SERVERRUN] = OK;
	sMain.aThread[THR_SERVERRUN] = std::thread(&CServer::Run, &Server);

	// start keyboard control thread
	sMain.aThreadStatus[THR_KEYBOARDCONTROL] = OK;
	sMain.aThread[THR_KEYBOARDCONTROL] = std::thread(&main_Keyboardcontrol, &sMain);

	// main loop
	while (1)
	{
		// check thread status
		for (i = 0; i < ARRAYSIZE(sMain.aThread); ++i)
		{
			if (sMain.aThreadStatus[i] != OK)
			{
				Log.Log("%s: There was an error in thread %d, ending...", __FUNCTION__, i);
				hasError = true;
				break;
			}
		}

		// check if application should exit
		if (hasError || sMain.exitApplication)
		{
			Log.Log("Exiting application...");

			retval = main_ExitApplication(&sMain, &Server, &Log);
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

int main_Keyboardcontrol(S_MAIN *psMain)
{
	char ch = 0;

	while (1)
	{
		// keyboard input
		ch = getchar();

		if (ch == 'e')
			psMain->exitApplication = true;

		usleep(DELAY_KEYBOARDCONTROLLOOP);
	}

	return OK;
}

int main_ExitApplication(S_MAIN *psMain, CServer *pServer, CLog *pLog)
{
	int i = 0;
	int retval = 0;
	int hasError = false;

	// exit server
	retval = pServer->OnExitApplication();
	if (retval != OK)
		hasError = true;

	// cancel main threads
	for (i = 0; i < ARRAYSIZE(psMain->aThread); ++i)
	{
		retval = CCore::DetachThreadSafely(&psMain->aThread[i]);
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