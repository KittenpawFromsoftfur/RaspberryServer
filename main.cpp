#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "mainlogic.h"

/* TODO
	GENERAL
		autostart file / bild needs update for main parameters
		What happens to CPU if we launch per autostart?
		Clean up everything / Sort functions properly / Code formatter { 0 }
		readme details about parameters and other stuffs...
		set_.../clear_... --> set .../clear ...
		gpio/mosfet --> set single or multiple
		gpio/mosfet --> read single or multiple
		Test every single function again

	BUGS
		writekey: when less lines than needed, it will take the last line instead of throwing error or making the necessary amount of lines

	INTERFACE
		Make it so you can connect with a browser and click on buttons instead of sending commands

	END
		Documentation please!
*/

int main(int argc, char *argv[])
{
	char *pParam = 0;
	char *pParamValue = 0;
	bool failed = false;
	int paramServerPort = -1;
	int paramUppercaseResponse = 0;

	for (int i = 1; i < argc; ++i)
	{
		// determine param
		pParam = argv[i];

		// determine param value
		pParamValue = 0;

		if (argc > i + 1)
			pParamValue = argv[i + 1];

		// compare params
		if (strncmp(pParam, MAINPARAM_PORT, MAX_LEN_MAINPARAMS) == 0)
		{
			if (pParamValue)
			{
				paramServerPort = atoi(pParamValue);
				i++;
			}
			else
			{
				failed = true;
			}
		}
		else if (strncmp(pParam, MAINPARAM_UPPERCASERESPONSE, MAX_LEN_MAINPARAMS) == 0)
		{
			if (pParamValue)
			{
				paramUppercaseResponse = atoi(pParamValue);
				i++;
			}
			else
			{
				failed = true;
			}
		}
		else
		{
			failed = true;
		}

		if (failed)
			break;
	}

	if (failed)
	{
		printf("\nInvalid parameters, usage:", argv[0]);
		printf("\n\n" HEADER_LINE);
		printf("\n%s... %s", MAINPARAM_PORT, DESCRIPTION_PARAM_PORT);
		printf("\n%s... %s", MAINPARAM_UPPERCASERESPONSE, DESCRIPTION_PARAM_UPPERCASERESPONSE);
		printf("\n" HEADER_LINE "\n\n");
		return ERROR;
	}

	CMainlogic mainlogic(LOG_FOLDERPATH, LOG_NAME, paramServerPort, paramUppercaseResponse);

	return mainlogic.EntryPoint();
}