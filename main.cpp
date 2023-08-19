/**
 * @file main.cpp
 * @author KittenpawFromsoftfur (finbox.entertainment@gmail.com)
 * @brief Entry point of the program, handles program call parameters and lists usage help
 * @version 1.0
 * @date 2023-08-18
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "mainlogic.h"

/* TODO
	INTERFACE
		Make it so you can connect with a browser and click on buttons instead of sending commands

	FAR FUTURE
		IO set multiple
		IO read multiple (https://stackoverflow.com/questions/43116/how-can-i-run-an-external-program-from-c-and-parse-its-output)
*/

/**
 * @brief Entry point of the program, handles program call parameters and lists usage help
 *
 * @param argc	Argument count
 * @param argv	Argument value array
 *
 * @return CMainlogic::EntryPoint() return value
 * @return ERROR
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