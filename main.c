#include <signal.h>
#include <unistd.h>

#include "server.h"
#include "log.h"
#include "core.h"
#include "main.h"

/* TODO
	GPIO
		set pinmode
		read pin
		set/read multiple pins at once
		
	MOSFET
		Implement function "mosread"
*/

// global variables
S_MAIN gsMain = { 0 };

// private functions
void *main_ThrfKeyboardcontrol(void *pArgs);
int main_ExitApplication();

int main()
{
	int retval = 0;
	struct sigaction sigAction = { 0 };
	int hasError = false;
	int i = 0;
	void *pFunc = 0;
	
	// create log folder path
	retval = core_Mkdir(FOLDERPATH_LOG);
	if (retval != OK)
	{
		printf("%s: Failed to make folder \"%s\"\n", FOLDERPATH_LOG);
		return ERROR;
	}
	
	snprintf(gsMain.aFilepathLog, ARRAYSIZE(gsMain.aFilepathLog), "%s/%s", FOLDERPATH_LOG, LOG_NAME);
	
	// initialize log
	retval = log_Init(gsMain.aFilepathLog);
	if (retval != OK)
		return ERROR;
		
	// make process ignore SIGPIPE signal, so it will not exit when writing to disconnected socket
	sigAction.sa_handler = SIG_IGN;
	retval = sigaction(SIGPIPE, &sigAction, NULL);
	if (retval != 0)
	{
		Log("%s: Failed to ignore the SIGPIPE signal", __FUNCTION__);
		return ERROR;
	}
	
	// run server and keybordcontrol thread
	for(i = 0; i < ARRAYSIZE(gsMain.aThread); ++i)
	{
		if (i == 0)
			pFunc = server_ThrfRun;
		else if (i == 1)
			pFunc = main_ThrfKeyboardcontrol;
		
		gsMain.aThreadStatus[i] = OK;
		retval = pthread_create(&gsMain.aThread[i], NULL, pFunc, NULL);
		if (retval != 0)
		{
			Log("%s: Failed to create thread %d", __FUNCTION__, i);
			return ERROR;
		}
	}
	
	// main loop
	while(1)
	{
		// check thread status
		for(i = 0; i < ARRAYSIZE(gsMain.aThread); ++i)
		{
			if (gsMain.aThreadStatus[i] != OK)
			{
				Log("%s: There was an error in thread %d, ending...", __FUNCTION__, i);
				hasError = true;
				break;
			}
		}
		
		// check if application should exit
		if (hasError || gsMain.exitApplication)
		{
			Log("Exiting application...");
			
			retval = main_ExitApplication();
			if (retval != OK)
				return ERROR;
			
			break;
			
			break;
		}
		
		usleep(DELAY_MAINLOOP);
	}
	
	Log("Application end");
	
	if (hasError)
		return ERROR;
	
	return OK;
}

void *main_ThrfKeyboardcontrol(void *pArgs)
{
	char ch = 0;
	
	while(1)
	{	
		// keyboard input
		ch = getchar();
		
		if (ch == 'e')
			gsMain.exitApplication = true;
		
		usleep(DELAY_KEYBOARDCONTROLLOOP);
	}

	return (void*)OK;
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
		Log("%s: Failed to create thread for exiting server applications", __FUNCTION__, i);
		hasError = true;
	}
	
	// join thread and wait for finish
	pthread_join(thrServerExitApp, (void**)&retval);
	if (retval != OK)
		hasError = true;
	
	// cancel main threads
	for(i = 0; i < ARRAYSIZE(gsMain.aThread); ++i)
	{
		retval = pthread_cancel(gsMain.aThread[i]);
		if (retval != 0)
		{
			Log("%s: There was an error cancelling thread %d", __FUNCTION__, i);
			hasError = true;
		}
	}
	
	if (hasError)
		return ERROR;
	
	return OK;
}