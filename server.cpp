#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <sys/select.h>

#include "log.h"
#include "core.h"
#include "hardware.h"
#include "server.h"
#include "main.h"

// global variables
S_COMMAND gasCommands[] =
{
	{ COM_HELP, COMMAND_STRING_HELP, "Lists this help", "", "", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_LOGGEDOUT | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_LOGGEDOUT },
	{ COM_ACTIVATEACCOUNT, "i", "Activates your account and unlocks all commands", COM_ACTIVATE_PASS1 " " COM_ACTIVATE_PASS2 " " COM_ACTIVATE_PASS3, "", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN },
	{ COM_REGISTER, "register", "Registers an account, " STRINGIFY_VALUE(MIN_LEN_CREDENTIALS) " - " STRINGIFY_VALUE(MAX_LEN_CREDENTIALS) " characters", "<username> <password>", "bob ross", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_LOGGEDOUT | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_LOGGEDOUT },
	{ COM_LOGIN, "login", "Logs an account in, " STRINGIFY_VALUE(MIN_LEN_CREDENTIALS) " - " STRINGIFY_VALUE(MAX_LEN_CREDENTIALS) " characters", "<username> <password>", "bob ross", COMFLAG_VISI_LOGGEDOUT | COMFLAG_EXEC_LOGGEDOUT },
	{ COM_LOGOUT, "logout", "Logs an account out", "", "", COMFLAG_VISI_LOGGEDIN | COMFLAG_EXEC_LOGGEDIN },
	{ COM_SHUTDOWN, "shutdown", "Shuts down the server", "", "", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_ACTIVATEDONLY },
	{ COM_RUN, "run", "Runs a console command on the machine", "<command>", "reboot", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_ACTIVATEDONLY },
	{ COM_DELETE, "delete", "Deletes the program log, your defines, your account or all files", "log/defines/account/all", "log", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_ACTIVATEDONLY },
	{ COM_DEFINE, "define", "Defines the name of a gpio pin. The name must begin with a letter, and have " STRINGIFY_VALUE(MIN_LEN_DEFINES) " - " STRINGIFY_VALUE(MAX_LEN_DEFINES) " characters. Name is case insensitive", "<pin number " COM_GPIO_RANGE "> <name>", "0 fan", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_ACTIVATEDONLY },
	{ COM_SET, "set", "Sets the status of a gpio pin, all pins are output", "<pin number " COM_GPIO_RANGE "/name> <1/0/on/off/high/low>", "fan on", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_ACTIVATEDONLY },
	{ COM_CLEAR, "clear", "Clears the status of all gpio pins", "", "", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_ACTIVATEDONLY },
	{ COM_MOSDEFINE, "mosdefine", "Defines the name of a mosfet. The name must begin with a letter, and have " STRINGIFY_VALUE(MIN_LEN_DEFINES) " - " STRINGIFY_VALUE(MAX_LEN_DEFINES) " characters. Name is case insensitive", "<mosfet number " COM_MOSFET_RANGE "> <name>", "1 lawnmower", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_ACTIVATEDONLY },
	{ COM_MOSSET, "mosset", "Sets the status of a mosfet", "<mosfet number " COM_MOSFET_RANGE "/name> <1/0/on/off/high/low>", "lawnmower on", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_ACTIVATEDONLY },
	{ COM_MOSREAD, "mosread", "(Not yet implemented!) Reads the status of all mosfets", "", "", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_ACTIVATEDONLY },
	{ COM_MOSCLEAR, "mosclear", "Clears the status of all mosfets", "", "", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_ACTIVATEDONLY | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_ACTIVATEDONLY },
	{ COM_ECHO, "echo", "The echo which echoes", "", "", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_LOGGEDOUT | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_LOGGEDOUT },
	{ COM_EXIT, "exit", "Closes your connection", "", "", COMFLAG_VISI_LOGGEDIN | COMFLAG_VISI_LOGGEDOUT | COMFLAG_EXEC_LOGGEDIN | COMFLAG_EXEC_LOGGEDOUT },
};

char gaaEchoes[][MAX_LEN_ECHO] =
{
	{ "Ahuurr durr" },
	{ "Yodelihuhuu" },
	{ "AAAAAAAAAAAH" },
	{ "Icy fire" },
	{ "Hello" },
	{ "Don't you scream at me lil boi" },
	{ "Stop" },
	{ "I said don't touch me... EEK" },
	{ "Meow" },
	{ "Bark" },
	{ "Echo" },
	{ "HADOUKEN" },
	{ "Hypopotomonstrosesquipedaliophobia" },
	{ "Mosquito bun" },
	{ "A: Will you stop permanently loudmouthing me? B: Yesn't" },
	{ "A: What's your name? B: Vanessa! A: No your real name! B: Vanessa! No your real name! B: FRAAAANK!" },
	{ "Hey... did you hear that?" },
	{ "Shush there's something in the bushes!" },
	{ "EEEEY I SAID STOP NO SCREAMING. WHY IS IT STILL SO LOUD? Oh wait thats me." },
	{ "You sussy baka" },
	{ "Yes please?" },
	{ "Yarr I'm a Pirate, I am a Pirate" },
};

S_SERVERINFO gsServerInfo = { 0 };
S_SLOTINFO gasSlotInfo[MAX_SLOTS] = { { 0 } };

void *CServer::ThrfRun(void *pArgs)
{
	int retval = 0;
	int connfdCurrent = 0;
	struct sockaddr_in sockaddr_current = { 0 };
	socklen_t socklenCurrent = 0;
    struct sockaddr_in serv_addr = { 0 };
	int i = 0;
	int slotIndex = 0;
	int sockOptValue = 0;

	// reset hardware
	retval = server_ResetHardware();
	if (retval != OK)
	{
		gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
		return (void*)ERROR;
	}

	// initialize hardware
	hardware_Init();

	// randomize for echo
	srand(time(0));

	// create accounts folder paths
	retval = core_Mkdir(FOLDERPATH_ACCOUNTS);
	if (retval != OK)
	{
		Log("%s: Failed to make folder \"%s\"", FOLDERPATH_ACCOUNTS);
		gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
		return (void*)ERROR;
	}

	// create defines gpio folder path
	retval = core_Mkdir(FOLDERPATH_DEFINES_GPIO);
	if (retval != OK)
	{
		Log("%s: Failed to make folder \"%s\"", FOLDERPATH_DEFINES_GPIO);
		gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
		return (void*)ERROR;
	}

	// create defines mosfet folder path
	retval = core_Mkdir(FOLDERPATH_DEFINES_MOSFET);
	if (retval != OK)
	{
		Log("%s: Failed to make folder \"%s\"", FOLDERPATH_DEFINES_MOSFET);
		gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
		return (void*)ERROR;
	}

	// create update thread
	retval = pthread_create(&gsServerInfo.thrUpdate, NULL, server_ThrfUpdate, NULL);
	if (retval != 0)
	{
		Log("%s: Failed to create update thread", __FUNCTION__);
		gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
		return (void*)ERROR;
	}

	// create server socket
    gsServerInfo.listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (gsServerInfo.listenfd == -1)
	{
		Log("%s: Failed to create listenfd, errno: %d", __FUNCTION__, errno);
		gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
		return (void*)ERROR;
	}

	// server socket option: allow reusing socket
	sockOptValue = 1;
	retval = setsockopt(gsServerInfo.listenfd, SOL_SOCKET, SO_REUSEADDR, &sockOptValue, sizeof(sockOptValue));
	if (retval != 0)
	{
		Log("%s: Failed to set socket option: SO_REUSEADDR, errno: %d", __FUNCTION__, errno);
		gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
		return (void*)ERROR;
	}

	// set up server settings
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(NET_SERVER_PORT);

	// bind server socket with settings
    retval = bind(gsServerInfo.listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	if (retval != 0)
	{
		Log("%s: Failed to bind, errno: %d", __FUNCTION__, errno);
		gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
		return (void*)ERROR;
	}

	// start listening
    retval = listen(gsServerInfo.listenfd, MAX_LEN_LISTENQUEUE);
	if (retval != 0)
	{
		Log("%s: Failed to listen, errno: %d", __FUNCTION__, errno);
		gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
		return (void*)ERROR;
	}

	// server loop
	Log("Server running...");

    while(1)
    {
		// accept incoming connections
		Log("Ready for new connection...");

		socklenCurrent = sizeof(struct sockaddr);
		connfdCurrent = accept(gsServerInfo.listenfd, (struct sockaddr*)&sockaddr_current, &socklenCurrent);

		// check connection details
		Log("Accepted port %d, IP %s", htons(sockaddr_current.sin_port), inet_ntoa(sockaddr_current.sin_addr));

		// check if a client with the same IP has been connected before
		slotIndex = -1;

		for(i = 0; i < ARRAYSIZE(gasSlotInfo); ++i)
		{
			if (sockaddr_current.sin_addr.s_addr == gasSlotInfo[i].clientIP)
			{
				slotIndex = i;
				break;
			}
		}

		// same IP, clean up old thread
		if (slotIndex > -1)
		{
			Log("Slot[%d] Same IP detected, cleaning up...", slotIndex);
			server_CleanOldSession(&gasSlotInfo[slotIndex]);
		}

		// find first free client index
		slotIndex = -1;

		for(i = 0; i < ARRAYSIZE(gasSlotInfo); ++i)
		{
			if (gasSlotInfo[i].clientIP == 0)
			{
				slotIndex = i;
				break;
			}
		}

		// if no free client index was found
		if (slotIndex < 0)
		{
			Log("Max client connections reached, no thread created");
			close(connfdCurrent);
			continue;
		}

		// client socket option: enable keepalive
        sockOptValue = 1;
        retval = setsockopt(connfdCurrent, SOL_SOCKET, SO_KEEPALIVE, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			Log("%s: Failed to set socket option: SO_KEEPALIVE, errno: %d", __FUNCTION__, errno);
			close(connfdCurrent);
			gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
			return (void*)ERROR;
		}

        // client socket option: keepalive idle time
        sockOptValue = 10;
        retval = setsockopt(connfdCurrent, IPPROTO_TCP, TCP_KEEPIDLE, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			Log("%s: Failed to set socket option: TCP_KEEPIDLE, errno: %d", __FUNCTION__, errno);
			close(connfdCurrent);
			gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
			return (void*)ERROR;
		}

        // client socket option: keepalive interval
        sockOptValue = 10;
        retval = setsockopt(connfdCurrent, IPPROTO_TCP, TCP_KEEPINTVL, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			Log("%s: Failed to set socket option: TCP_KEEPINTVL, errno: %d", __FUNCTION__, errno);
			close(connfdCurrent);
			gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
			return (void*)ERROR;
		}

        // client socket option: keepalive probes
        sockOptValue = 2;
        retval = setsockopt(connfdCurrent, IPPROTO_TCP, TCP_KEEPCNT, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			Log("%s: Failed to set socket option: TCP_KEEPCNT, errno: %d", __FUNCTION__, errno);
			close(connfdCurrent);
			gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
			return (void*)ERROR;
		}

		// add IP to list
		gasSlotInfo[slotIndex].clientIP = sockaddr_current.sin_addr.s_addr;

		// assign connection socket
		gasSlotInfo[slotIndex].connfd = connfdCurrent;

		// create thread
		Log("Slot[%d] Creating new thread...", slotIndex);

		gasSlotInfo[slotIndex].threadParamsClientConnection.sockfd = gasSlotInfo[slotIndex].connfd;
		gasSlotInfo[slotIndex].threadParamsClientConnection.slotIndex = slotIndex;
		retval = pthread_create(&gasSlotInfo[slotIndex].thrClientConnection, NULL, server_ThrfClientConnection, (void*)&gasSlotInfo[slotIndex].threadParamsClientConnection);
		if (retval != 0)
		{
			Log("%s: Slot[%d] Failed to create client connection thread", __FUNCTION__, slotIndex);
			close(gasSlotInfo[slotIndex].connfd);
			gsMain.aThreadStatus[THR_SERVERRUN] = ERROR;
			return (void*)ERROR;
		}
	}

	return (void*)OK;
}

void *CServer::ThrfClientConnection(void *pArgs)
{
	int retval = 0;
	S_PARAMS_CLIENTCONNECTION *psParams = (S_PARAMS_CLIENTCONNECTION*)pArgs;
	char aBufRaw[NET_BUFFERSIZE] = { 0 };
	char aBuf[NET_BUFFERSIZE] = { 0 };
	char aBufResp[NET_BUFFERSIZE] = { 0 };
	S_SLOTINFO *psSlotInfo = &gasSlotInfo[psParams->slotIndex];
	struct tm sTime = { 0 };
	struct timeval sTimeval = { 0 };
	fd_set sFDSet = { 0 };
	int timeoutOccurred = false;

	// check if IP banned, then close socket
	if (server_CheckIPBanned(psSlotInfo, &sTime))
	{
		Log("Slot[%d] IP '%s' still banned for %dh %dm %ds, cleaning up...", psParams->slotIndex, inet_ntoa(server_GetIPStruct(psSlotInfo->clientIP)), sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
		snprintf(aBufResp, ARRAYSIZE(aBufResp), RESPONSE_PREFIX "You are banned for %dh %dm %ds\n", sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
		write(psParams->sockfd, aBufResp, ARRAYSIZE(aBufResp));
		server_CleanOldSession(psSlotInfo);
		return (void*)ERROR;
	}

	// read from client loop
	while(1)
	{
		// write response prefix to client
		if (!timeoutOccurred)
			write(psParams->sockfd, RESPONSE_PREFIX, ARRAYSIZE(RESPONSE_PREFIX));

		// add server file descriptor to select watch list
		FD_ZERO(&sFDSet);
		FD_SET(psParams->sockfd, &sFDSet);

		// set select timeout
		memset(&sTimeval, 0, sizeof(sTimeval));
		sTimeval.tv_sec = TIMEOUT_CLIENTCONNECTION_READ;

		// read from connection until bytes are read
		retval = select(psParams->sockfd + 1, &sFDSet, NULL, NULL, &sTimeval);

		// data available
		if (retval > 0)
		{
			timeoutOccurred = false;

			memset(aBufRaw, 0, ARRAYSIZE(aBufRaw));
			retval = read(psParams->sockfd, aBufRaw, ARRAYSIZE(aBufRaw));

			// copy aBuf and ignore certain characters
			memset(aBuf, 0, ARRAYSIZE(aBuf));
			core_StringCopyIgnore(aBuf, aBufRaw, ARRAYSIZE(aBuf), "\r\n");

			// check if client disconnected and close socket
			if (strnlen(aBufRaw, ARRAYSIZE(aBuf)) == 0)
			{
				Log("Slot[%d] Disconnected", psParams->slotIndex);

				server_CleanOldSession(psSlotInfo);
				return (void*)ERROR;
			}
			else if (strnlen(aBuf, ARRAYSIZE(aBuf)) > 0)// client sent message
			{
				Log("Slot[%d] <%s>", psParams->slotIndex, aBuf);

				memset(aBufResp, 0, ARRAYSIZE(aBufResp));
				retval = server_ParseMessage(psParams, aBuf, aBufResp, ARRAYSIZE(aBufResp));
				if (retval != OK)
				{
					Log("%s: Slot[%d] Error parsing message", __FUNCTION__, psParams->slotIndex);
				}

				// send response
				if (strlen(aBufResp) > 0)
					write(psParams->sockfd, aBufResp, ARRAYSIZE(aBufResp));

				// check if slot requested disconnect or has been banned and close socket
				if (psSlotInfo->requestedDisconnect || psSlotInfo->banned)
				{
					server_CleanOldSession(psSlotInfo);
					return (void*)ERROR;
				}

				usleep(DELAY_RECEIVE);
			}
		}
		else if (retval == 0)// timeout
		{
			timeoutOccurred = true;
		}
		else// error
		{
			Log("Slot[%d] Error for select(), errno: %d\n", psParams->slotIndex, errno);
		}
	}

	return (void*)OK;
}

void *CServer::ThrfUpdate(void *pArgs)
{
	time_t currentTime = 0;
	int i = 0;

	while(1)
	{
		// get current time
		currentTime = time(0);

		// update IP bans
		for(i = 0; i < ARRAYSIZE(gsServerInfo.aBannedIPs); ++i)
		{
			if (gsServerInfo.aBannedIPs[i] == 0)
				continue;

			if (currentTime >= gsServerInfo.aBanStartTime[i] + gsServerInfo.aBanDuration[i])
			{
				// remove IP from suspicious list
				server_RemoveSuspiciousIP(gsServerInfo.aBannedIPs[i]);

				// unban IP
				Log("IP[%d] '%s' has been unbanned", i, inet_ntoa(server_GetIPStruct(gsServerInfo.aBannedIPs[i])));
				gsServerInfo.aBannedIPs[i] = 0;
				gsServerInfo.aBanStartTime[i] = 0;
				gsServerInfo.aBanDuration[i] = 0;
			}
		}

		// update suspicious IPs list
		for(i = 0; i < ARRAYSIZE(gsServerInfo.aSuspiciousIPs); ++i)
		{
			if (gsServerInfo.aSuspiciousIPs[i] == 0)
				continue;

			if (currentTime >= gsServerInfo.aSuspiciousStartTime[i] + SUSPICIOUS_FALLOFF_TIME)
			{
				Log("Unlisted suspicious IP[%d] '%s'", i, inet_ntoa(server_GetIPStruct(gsServerInfo.aSuspiciousIPs[i])));
				server_RemoveSuspiciousIP(gsServerInfo.aSuspiciousIPs[i]);
			}
		}

		usleep(DELAY_UPDATELOOP);
	}
}

void *CServer::ThrfOnExitApplication(void *pArgs)
{
	int i = 0;
	int retval = 0;
	int hasError = false;

	retval = server_ResetHardware();
	if (retval != OK)
		hasError = true;

	// cancel update thread
	pthread_cancel(gsServerInfo.thrUpdate);

	// cancel client connection threads and close sockets
	for(i = 0; i < ARRAYSIZE(gasSlotInfo); ++i)
	{
		pthread_cancel(gasSlotInfo[i].thrClientConnection);
		server_CloseClientSocket(&gasSlotInfo[i]);
	}

	// close server socket
	close(gsServerInfo.listenfd);

	if (hasError)
		return (void*)ERROR;

	return (void*)OK;
}

int CServer::ParseMessage(S_PARAMS_CLIENTCONNECTION *psParams, const char *pMsg, char *pResp, size_t LenResp)
{
	int retval = 0;
	char *pToken = 0;
	char *pRest = 0;
	char aaToken[MAX_TOKENS][MAX_LEN_TOKEN] = { { 0 } };
	char aMessage[NET_BUFFERSIZE] = { 0 };
	int tokenIndex = 0;

	strncpy(aMessage, pMsg, ARRAYSIZE(aMessage));
	pRest = aMessage;

	while(1)
	{
		pToken = strtok_r(pRest, " ", &pRest);

		if (!pToken)
			break;

		strncpy(aaToken[tokenIndex], pToken, ARRAYSIZE(aaToken[0]));
		tokenIndex++;

		if (tokenIndex >= ARRAYSIZE(aaToken))
			break;
	}

	// evaluate tokens
	server_EvaluateTokens(psParams, aaToken, pResp, LenResp, pMsg);

	return OK;
}

void CServer::EvaluateTokens(S_PARAMS_CLIENTCONNECTION *psParams, char aaToken[MAX_TOKENS][MAX_LEN_TOKEN], char *pResp, size_t LenResp, const char *pMsgFull)
{
	int retval = 0;
	int foundMatch = false;
	int commandIndex = 0;
	char aBufTemp[NET_BUFFERSIZE] = { 0 };
	int randNr = 0;
	S_SLOTINFO *psSlotInfo = &gasSlotInfo[psParams->slotIndex];
	int commandExecutable = false;
	int commandVisible = false;
	int commandCorrect = false;
	int respondCommandUnknown = false;
	int respondParametersWrong = false;
	int i = 0;
	char aRead[MAX_LEN_LINES] = { 0 };
	int pinNumber = 0;
	int pinState = 0;
	int isName = 0;
	int wrongCredentials = false;
	int doBan = false;
	time_t banTime = 0;
	struct tm sTime = { 0 };
	int susAttempts = 0;
	char aFullMessage[NET_BUFFERSIZE] = { 0 };
	char aFullParameters[NET_BUFFERSIZE] = { 0 };
	char *pRest = 0;
	char aSystemString[MAX_LEN_SYSTEMCOMMAND] = { 0 };

	// look for command
	for(i = 0; i < ARRAYSIZE(gasCommands); ++i)
	{
		if (core_StringCompareNocase(aaToken[0], gasCommands[i].aName, ARRAYSIZE(aaToken[0])) == 0)
		{
			foundMatch = true;
			commandIndex = i;
			break;
		}
	}

	/* NOTE
		Response prefix "> " and newline "\n" are added automatically
	*/

	// add response prefix to buffer
	strncpy(pResp, RESPONSE_PREFIX, LenResp);

	// command was found
	if (foundMatch)
	{
		// check if command is executable
		commandExecutable = server_IsCommandExecutable(psSlotInfo, gasCommands[commandIndex].flags);

		// check if command is visible
		commandVisible = server_IsCommandVisible(psSlotInfo, gasCommands[commandIndex].flags);
	}

	if (commandExecutable)
	{
		Log("Slot[%d] --> '%s'", psParams->slotIndex, gasCommands[commandIndex].aName);

		switch(gasCommands[commandIndex].ID)
		{
			case COM_HELP:
				strncat(pResp, "****** Help ******\n" RESPONSE_PREFIX "All commands are case insensitive.\n", LenResp);

				// if logged in, inform as who
				if (psSlotInfo->loggedIn)
				{
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), RESPONSE_PREFIX "Logged in as '%s'.\n", psSlotInfo->aUsername);
					strncat(pResp, aBufTemp, LenResp);
				}

				// if account activated, inform
				if (psSlotInfo->activated)
				{
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), RESPONSE_PREFIX "Account is activated.\n" RESPONSE_PREFIX "\n");
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					strncat(pResp, RESPONSE_PREFIX "\n", LenResp);
				}

				for(int i = 0; i < ARRAYSIZE(gasCommands); ++i)
				{
					if (!server_IsCommandVisible(psSlotInfo, gasCommands[i].flags))
						continue;

					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), RESPONSE_PREFIX "%s... %s. Usage: '%s%s%s'.", gasCommands[i].aName, gasCommands[i].aHelp, gasCommands[i].aName, strnlen(gasCommands[i].aParams, ARRAYSIZE(gasCommands[0].aParams)) > 0 ? " " : "", gasCommands[i].aParams);
					strncat(pResp, aBufTemp, LenResp);

					// don't give example if no params needed
					if (strnlen(gasCommands[i].aExample, ARRAYSIZE(gasCommands[0].aExample)) > 0)
					{
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), " Example: '%s %s'\n", gasCommands[i].aName, gasCommands[i].aExample);
						strncat(pResp, aBufTemp, LenResp);
					}
					else
					{
						strncat(pResp, "\n", LenResp);
					}
				}

				strncat(pResp, RESPONSE_PREFIX "******************", LenResp);
			break;

			case COM_ACTIVATEACCOUNT:
				// check if the passphrase is correct
				if (core_StringCompareNocase(aaToken[1], COM_ACTIVATE_PASS1, ARRAYSIZE(aaToken[0])) == 0 &&
					core_StringCompareNocase(aaToken[2], COM_ACTIVATE_PASS2, ARRAYSIZE(aaToken[0])) == 0 &&
					core_StringCompareNocase(aaToken[3], COM_ACTIVATE_PASS3, ARRAYSIZE(aaToken[0])) == 0)
					commandCorrect = true;

				if (commandCorrect)
				{
					retval = server_WriteKey(psSlotInfo, FILETYPE_ACCOUNT, ACCKEY_ACTIVATED, "1", pResp, LenResp);
					if (retval != OK)
					{
						Log("Slot[%d] Failed to activate account '%s'", psParams->slotIndex, psSlotInfo->aUsername);
					}
					else
					{
						psSlotInfo->activated = true;
						strncat(pResp, COM_ACTIVATE_RESPONSE "! Account activated!", LenResp);
						Log("Slot[%d] Account '%s' has been activated", psParams->slotIndex, psSlotInfo->aUsername);
					}
				}
				else
				{
					respondCommandUnknown = true;
				}
			break;

			case COM_REGISTER:
				retval = server_AccountAction(ACCACTION_REGISTER, psSlotInfo, aaToken[1], aaToken[2], NULL, pResp, LenResp);
				if (retval != OK)
				{
					Log("Slot[%d] Failed to register <%s> <%s>", psParams->slotIndex, aaToken[1], aaToken[2]);
				}
				else
				{
					Log("Slot[%d] Registered user <%s> <%s>", psParams->slotIndex, aaToken[1], aaToken[2]);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Successfully registered, please log in!");
					strncat(pResp, aBufTemp, LenResp);
				}
			break;

			case COM_LOGIN:
				retval = server_AccountAction(ACCACTION_LOGIN, psSlotInfo, aaToken[1], aaToken[2], &wrongCredentials, pResp, LenResp);
				if (retval != OK)
				{
					if (wrongCredentials)// if the reason for fail is "wrong credentials"
					{
						psSlotInfo->failedLogins++;
						retval = server_AddSuspiciousIP(psSlotInfo->clientIP);
						if (retval != OK)
						{
							Log("Slot[%d] Unable to add suspicious IP '%s', safety mesure: Exit application", psParams->slotIndex, inet_ntoa(server_GetIPStruct(psSlotInfo->clientIP)));
							gsMain.exitApplication = true;
						}
					}

					susAttempts = server_GetSuspiciousAttempts(psSlotInfo->clientIP);
					Log("Slot[%d] Failed to log in with '%s' '%s', failed logins: %d/%d, total attempts of '%s': %d/%d", psParams->slotIndex, aaToken[1], aaToken[2], psSlotInfo->failedLogins, MAX_FAILED_LOGINS, inet_ntoa(server_GetIPStruct(psSlotInfo->clientIP)), susAttempts, MAX_FAILED_LOGINS_SUSPICIOUS);

					// check if IP ban necessary
					if (susAttempts >= MAX_FAILED_LOGINS_SUSPICIOUS)
					{
						doBan = true;
						banTime = IP_BAN_DURATION_SUSPICIOUS;
					}
					else if (psSlotInfo->failedLogins >= MAX_FAILED_LOGINS)
					{
						doBan = true;
						banTime = IP_BAN_DURATION_FAILEDLOGINS;
					}

					// check if IP ban necessary by failed attempts
					if (doBan)
					{
						retval = server_BanIP(psSlotInfo->clientIP, banTime);
						if (retval != OK)
						{
							Log("Slot[%d] Unable to ban IP '%s', safety mesure: Exit application", psParams->slotIndex, inet_ntoa(server_GetIPStruct(psSlotInfo->clientIP)));
							gsMain.exitApplication = true;
						}
						else
						{
							psSlotInfo->banned = true;
							sTime = *localtime(&banTime);
							Log("Slot[%d] Banned IP '%s' for %dh %dm %ds", psParams->slotIndex, inet_ntoa(server_GetIPStruct(psSlotInfo->clientIP)), sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
							snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "\n" RESPONSE_PREFIX "You have been banned for %dh %dm %ds", sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
							strncat(pResp, aBufTemp, LenResp);
						}
					}
				}
				else
				{
					Log("Slot[%d] Account '%s' logged in", psParams->slotIndex, psSlotInfo->aUsername);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Welcome back %s", aaToken[1]);
					strncat(pResp, aBufTemp, LenResp);
				}
			break;

			case COM_LOGOUT:
				retval = server_AccountAction(ACCACTION_LOGOUT, psSlotInfo, NULL, NULL, NULL, pResp, LenResp);
				if (retval != OK)
				{
					Log("Slot[%d] Failed to log out with account '%s'", psParams->slotIndex, psSlotInfo->aUsername);
				}
				else
				{
					Log("Slot[%d] Account logged out", psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Logged out");
					strncat(pResp, aBufTemp, LenResp);
				}
			break;

			case COM_SHUTDOWN:
				Log("Slot[%d] Has shut down the server", psParams->slotIndex);
				gsMain.exitApplication = true;
			break;

			case COM_RUN:
				// remove command from parameters
				strncpy(aFullMessage, pMsgFull, ARRAYSIZE(aFullMessage));
				pRest = aFullMessage;
				strtok_r(pRest, " ", &pRest);
				strncpy(aFullParameters, pRest, ARRAYSIZE(aFullParameters));

				Log("Slot[%d] Running command '%s'...", psParams->slotIndex, aFullParameters);

				retval = system(aFullParameters);

				Log("Slot[%d] Command returned %d", psParams->slotIndex, retval);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "'%s' returned %d", aFullParameters, retval);
				strncat(pResp, aBufTemp, LenResp);
			break;

			case COM_DELETE:
				// log
				if (core_StringCompareNocase(aaToken[1], "log", ARRAYSIZE(aaToken[0])) == 0)
				{
					retval = server_RemoveFile(psSlotInfo, FILETYPE_LOG, pResp, LenResp);
					if (retval != OK)
					{
						Log("%s: Failed to remove log file", __FUNCTION__);
					}
					else
					{
						strncat(pResp, "Deleted log file", LenResp);
						Log("Slot[%d] User '%s' deleted log", psParams->slotIndex, psSlotInfo->aUsername);
					}
				}// account
				else if (core_StringCompareNocase(aaToken[1], "account", ARRAYSIZE(aaToken[0])) == 0)
				{
					// delete account
					retval = server_RemoveFile(psSlotInfo, FILETYPE_ACCOUNT, pResp, LenResp);
					if (retval != OK)
					{
						Log("%s: Failed to delete account file, defines were not removed", __FUNCTION__);
					}
					else
					{
						// delete defines gpio
						retval = server_RemoveFile(psSlotInfo, FILETYPE_DEFINES_GPIO, pResp, LenResp);
						if (retval != OK)
						{
							Log("%s: Failed to additionally delete defines gpio file", __FUNCTION__);
						}
						else
						{
							// delete defines mosfet
							retval = server_RemoveFile(psSlotInfo, FILETYPE_DEFINES_MOSFET, pResp, LenResp);
							if (retval != OK)
							{
								Log("%s: Failed to additionally delete defines mosfet file", __FUNCTION__);
							}
							else
							{
								// log out
								retval = server_AccountAction(ACCACTION_LOGOUT, psSlotInfo, NULL, NULL, NULL, pResp, LenResp);
								if (retval != OK)
								{
									Log("Slot[%d] Failed to additionally log out with account '%s'", psParams->slotIndex, psSlotInfo->aUsername);
								}
								else
								{
									Log("Slot[%d] Deleted account and logged out", psParams->slotIndex);
									snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Deleted account and logged out");
									strncat(pResp, aBufTemp, LenResp);
								}
							}
						}
					}
				}// defines are just cleared because the file is still needed
				else if (core_StringCompareNocase(aaToken[1], "defines", ARRAYSIZE(aaToken[0])) == 0)
				{
					// clear defines gpio
					retval = server_CreateDefinesFile(FILETYPE_DEFINES_GPIO, psSlotInfo->aUsername, pResp, LenResp);
					if (retval != OK)
					{
						Log("Slot[%d] User '%s' unable to clear defines gpio", psParams->slotIndex, psSlotInfo->aUsername);
					}
					else
					{
						// clear defines mosfet
						retval = server_CreateDefinesFile(FILETYPE_DEFINES_MOSFET, psSlotInfo->aUsername, pResp, LenResp);
						if (retval != OK)
						{
							Log("Slot[%d] User '%s' unable to clear defines mosfet", psParams->slotIndex, psSlotInfo->aUsername);
						}
						else
						{
							Log("Slot[%d] User '%s' defines were cleared", psParams->slotIndex, psSlotInfo->aUsername);
							snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Deleted defines");
							strncat(pResp, aBufTemp, LenResp);
						}
					}
				}// all
				else if (core_StringCompareNocase(aaToken[1], "all", ARRAYSIZE(aaToken[0])) == 0)
				{
					// safety measure in case we ever change the accounts / defines directory to anywhere that is not our program directory
					if (strstr(FOLDERPATH_ACCOUNTS, FOLDERPATH_FILESTASH_MINIMUM) == 0 || strstr(FOLDERPATH_DEFINES_GPIO, FOLDERPATH_FILESTASH_MINIMUM) == 0 || strstr(FOLDERPATH_DEFINES_MOSFET, FOLDERPATH_FILESTASH_MINIMUM) == 0)
					{
						Log("Slot[%d] Danger! Folder path for accounts or defines are not according to the minimum required filestash folder path '%s', could delete important files", psParams->slotIndex, FOLDERPATH_FILESTASH_MINIMUM);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Unable to delete all files", FOLDERPATH_ACCOUNTS);
						strncat(pResp, aBufTemp, LenResp);
					}
					else
					{
						// remove accounts
						retval = core_RemoveFilesDirectory(FOLDERPATH_ACCOUNTS);
						if (retval != OK)
						{
							Log("Slot[%d] Failed to remove files in directory '%s'", psParams->slotIndex, FOLDERPATH_ACCOUNTS);
							snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to remove files in directory '%s'", FOLDERPATH_ACCOUNTS);
							strncat(pResp, aBufTemp, LenResp);
						}
						else
						{
							// remove defines gpio
							retval = core_RemoveFilesDirectory(FOLDERPATH_DEFINES_GPIO);
							if (retval != OK)
							{
								Log("Slot[%d] Failed to remove files in directory '%s'", psParams->slotIndex, FOLDERPATH_DEFINES_GPIO);
								snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to remove files in directory '%s'", FOLDERPATH_DEFINES_GPIO);
								strncat(pResp, aBufTemp, LenResp);
							}
							else
							{
								// remove defines mosfet
								retval = core_RemoveFilesDirectory(FOLDERPATH_DEFINES_MOSFET);
								if (retval != OK)
								{
									Log("Slot[%d] Failed to remove files in directory '%s'", psParams->slotIndex, FOLDERPATH_DEFINES_GPIO);
									snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to remove files in directory '%s'", FOLDERPATH_DEFINES_GPIO);
									strncat(pResp, aBufTemp, LenResp);
								}
								else
								{
									// remove log
									retval = server_RemoveFile(psSlotInfo, FILETYPE_LOG, pResp, LenResp);
									if (retval != OK)
									{
										Log("Slot[%d] Failed to remove log file", psParams->slotIndex);
									}
									else
									{
										// log out
										retval = server_AccountAction(ACCACTION_LOGOUT, psSlotInfo, NULL, NULL, NULL, pResp, LenResp);
										if (retval != OK)
										{
											Log("Slot[%d] Failed to additionally log out with account '%s'", psParams->slotIndex, psSlotInfo->aUsername);
										}
										else
										{
											Log("Slot[%d] Deleted all files and logged out", psParams->slotIndex);
											snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Deleted all files and logged out");
											strncat(pResp, aBufTemp, LenResp);
										}
									}
								}
							}
						}
					}
				}
				else// invalid category
				{
					respondParametersWrong = true;
					Log("Slot[%d] Wrong parameters for deleting", psParams->slotIndex);
				}
			break;

			case COM_DEFINE:
				pinNumber = atoi(aaToken[1]);

				if (core_IsLetter(aaToken[1][0]))
				{
					Log("Slot[%d] Error, pin number must be a number", psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, pin number must be a number");
					strncat(pResp, aBufTemp, LenResp);
				}// number must be in range
				else if (!CHardware::IsGpioValid(pinNumber))
				{
					Log("Slot[%d] Error, pin number must be " COM_GPIO_RANGE, psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, pin number must be " COM_GPIO_RANGE);
					strncat(pResp, aBufTemp, LenResp);
				}// name must begin with letter
				else if (!core_IsLetter(aaToken[2][0]))
				{
					Log("Slot[%d] Error, name must begin with a letter", psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name must begin with a letter");
					strncat(pResp, aBufTemp, LenResp);
				}// name must be simple ascii
				else if (!core_CheckStringAscii(aaToken[2], ARRAYSIZE(aaToken[0])))
				{
					Log("Slot[%d] Error, name must contain only characters " CHARRANGE_ASCII_READABLE, psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name must contain only characters " CHARRANGE_ASCII_READABLE);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					if (strlen(aaToken[2]) < MIN_LEN_DEFINES || strlen(aaToken[2]) > MAX_LEN_DEFINES)
					{
						Log("Slot[%d] Error, define name must have %d - %d characters", psParams->slotIndex, MIN_LEN_DEFINES, MAX_LEN_DEFINES);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, define name must have %d - %d characters", MIN_LEN_DEFINES, MAX_LEN_DEFINES);
						strncat(pResp, aBufTemp, LenResp);
					}
					else if (!core_CheckStringAscii(aaToken[2], ARRAYSIZE(aaToken[2])))
					{
						Log("Slot[%d] Error, define name must only contain the characters " CHARRANGE_ASCII_READABLE, psParams->slotIndex);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, define name must only contain the characters " CHARRANGE_ASCII_READABLE);
						strncat(pResp, aBufTemp, LenResp);
					}
					else
					{
						retval = server_WriteKey(psSlotInfo, FILETYPE_DEFINES_GPIO, pinNumber, aaToken[2], pResp, LenResp);
						if (retval != OK)
						{
							Log("Slot[%d] Failed to define <%d> <%s>", psParams->slotIndex, pinNumber, aaToken[2]);
						}
						else
						{
							Log("Slot[%d] Defined pin %d as '%s'", psParams->slotIndex, pinNumber, aaToken[2]);
							snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Defined pin %d as '%s'", pinNumber, aaToken[2]);
							strncat(pResp, aBufTemp, LenResp);
						}
					}
				}
			break;

			case COM_SET:
				// check number
				isName = false;
				pinNumber = -1;

				// check if name is name or number
				if (core_IsLetter(aaToken[1][0]))
					isName = true;

				if (isName)
				{
					for (i = 0; i <= 31; ++i)
					{
						// skip non-gpio and i2c-1 pins
						if (!IsGpioValid(i))// ----------------------- TODO
							continue;

						retval = server_ReadKey(psSlotInfo->aUsername, FILETYPE_DEFINES_GPIO, i, aRead, ARRAYSIZE(aRead), pResp, LenResp);
						if (retval != OK)
						{
							Log("Slot[%d] Failed to read key %d", psParams->slotIndex, i);
						}
						else// compare name
						{
							if (core_StringCompareNocase(aRead, aaToken[1], ARRAYSIZE(aRead)) == 0)
							{
								pinNumber = i;
								break;
							}
						}
					}
				}
				else
				{
					pinNumber = atoi(aaToken[1]);
				}

				// check pin number
				if (!server_IsGpioValid(pinNumber))
				{
					Log("Slot[%d] Error, name not found or pin number outside " COM_GPIO_RANGE, psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name not found or pin number outside " COM_GPIO_RANGE);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					// check state
					isName = false;
					pinState = -1;

					// check if state is name or number (or empty)
					if (core_IsLetter(aaToken[2][0]) || strlen(aaToken[2]) == 0)
						isName = true;

					// check on/off/high/low
					if (isName)
					{
						if (core_StringCompareNocase(aaToken[2], "on", ARRAYSIZE(aaToken[2])) == 0)
							pinState = 1;
						else if (core_StringCompareNocase(aaToken[2], "off", ARRAYSIZE(aaToken[2])) == 0)
							pinState = 0;
						else if (core_StringCompareNocase(aaToken[2], "high", ARRAYSIZE(aaToken[2])) == 0)
							pinState = 1;
						else if (core_StringCompareNocase(aaToken[2], "low", ARRAYSIZE(aaToken[2])) == 0)
							pinState = 0;
					}
					else
					{
						pinState = atoi(aaToken[2]);
					}

					// check pin state validity
					if (pinState < 0 || pinState > 1)
					{
						Log("Slot[%d] Error unknown pin state '%s'", psParams->slotIndex, aaToken[2]);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error unknown pin state '%s'", aaToken[2]);
						strncat(pResp, aBufTemp, LenResp);
					}
					else
					{
						// set pin
						hardware_Set(pinNumber, pinState);

						Log("Slot[%d] Pin %s (%d) --> %s", psParams->slotIndex, aaToken[1], pinNumber, aaToken[2]);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Pin %s (%d) --> %s", aaToken[1], pinNumber, aaToken[2]);
						strncat(pResp, aBufTemp, LenResp);
					}
				}
			break;

			case COM_CLEAR:
				hardware_Clear();

				Log("Slot[%d] Cleared pins", psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Cleared pins");
				strncat(pResp, aBufTemp, LenResp);
			break;

			case COM_MOSDEFINE:
				pinNumber = atoi(aaToken[1]);

				if (core_IsLetter(aaToken[1][0]))
				{
					Log("Slot[%d] Error, mosfet number must be a number", psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, mosfet number must be a number");
					strncat(pResp, aBufTemp, LenResp);
				}// number must be in range
				else if (!server_IsMosfetValid(pinNumber))
				{
					Log("Slot[%d] Error, mosfet number must be " COM_MOSFET_RANGE, psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, mosfet number must be " COM_MOSFET_RANGE);
					strncat(pResp, aBufTemp, LenResp);
				}// name must begin with letter
				else if (!core_IsLetter(aaToken[2][0]))
				{
					Log("Slot[%d] Error, name must begin with a letter", psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name must begin with a letter");
					strncat(pResp, aBufTemp, LenResp);
				}// name must be simple ascii
				else if (!core_CheckStringAscii(aaToken[2], ARRAYSIZE(aaToken[0])))
				{
					Log("Slot[%d] Error, name must contain only characters " CHARRANGE_ASCII_READABLE, psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name must contain only characters " CHARRANGE_ASCII_READABLE);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					if (strlen(aaToken[2]) < MIN_LEN_DEFINES || strlen(aaToken[2]) > MAX_LEN_DEFINES)
					{
						Log("Slot[%d] Error, define name must have %d - %d characters", psParams->slotIndex, MIN_LEN_DEFINES, MAX_LEN_DEFINES);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, define name must have %d - %d characters", MIN_LEN_DEFINES, MAX_LEN_DEFINES);
						strncat(pResp, aBufTemp, LenResp);
					}
					else if (!core_CheckStringAscii(aaToken[2], ARRAYSIZE(aaToken[2])))
					{
						Log("Slot[%d] Error, define name must only contain the characters " CHARRANGE_ASCII_READABLE, psParams->slotIndex);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, define name must only contain the characters " CHARRANGE_ASCII_READABLE);
						strncat(pResp, aBufTemp, LenResp);
					}
					else
					{
						retval = server_WriteKey(psSlotInfo, FILETYPE_DEFINES_MOSFET, pinNumber, aaToken[2], pResp, LenResp);
						if (retval != OK)
						{
							Log("Slot[%d] Failed to define <%d> <%s>", psParams->slotIndex, pinNumber, aaToken[2]);
						}
						else
						{
							Log("Slot[%d] Defined mosfet %d as '%s'", psParams->slotIndex, pinNumber, aaToken[2]);
							snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Defined mosfet %d as '%s'", pinNumber, aaToken[2]);
							strncat(pResp, aBufTemp, LenResp);
						}
					}
				}
			break;

			case COM_MOSSET:
				// check number
				isName = false;
				pinNumber = -1;

				// check if name is name or number
				if (core_IsLetter(aaToken[1][0]))
					isName = true;

				if (isName)
				{
					for (i = 0; i <= 8; ++i)
					{
						retval = server_ReadKey(psSlotInfo->aUsername, FILETYPE_DEFINES_MOSFET, i, aRead, ARRAYSIZE(aRead), pResp, LenResp);
						if (retval != OK)
						{
							Log("Slot[%d] Failed to read key %d", psParams->slotIndex, i);
						}
						else// compare name
						{
							if (core_StringCompareNocase(aRead, aaToken[1], ARRAYSIZE(aRead)) == 0)
							{
								pinNumber = i;
								break;
							}
						}
					}
				}
				else
				{
					pinNumber = atoi(aaToken[1]);
				}

				// check mosfet number
				if (!server_IsMosfetValid(pinNumber))
				{
					Log("Slot[%d] Error, name not found or mosfet number outside " COM_MOSFET_RANGE, psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name not found or mosfet number outside " COM_MOSFET_RANGE);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					// check state
					isName = false;
					pinState = -1;

					// check if state is name or number (or empty)
					if (core_IsLetter(aaToken[2][0]) || strlen(aaToken[2]) == 0)
						isName = true;

					// check on/off/high/low
					if (isName)
					{
						if (core_StringCompareNocase(aaToken[2], "on", ARRAYSIZE(aaToken[2])) == 0)
							pinState = 1;
						else if (core_StringCompareNocase(aaToken[2], "off", ARRAYSIZE(aaToken[2])) == 0)
							pinState = 0;
						else if (core_StringCompareNocase(aaToken[2], "high", ARRAYSIZE(aaToken[2])) == 0)
							pinState = 1;
						else if (core_StringCompareNocase(aaToken[2], "low", ARRAYSIZE(aaToken[2])) == 0)
							pinState = 0;
					}
					else
					{
						pinState = atoi(aaToken[2]);
					}

					// check mosfet state validity
					if (pinState < 0 || pinState > 1)
					{
						Log("Slot[%d] Error unknown mosfet state '%s'", psParams->slotIndex, aaToken[2]);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error unknown mosfet state '%s'", aaToken[2]);
						strncat(pResp, aBufTemp, LenResp);
					}
					else
					{
						// set mosfet
						snprintf(aSystemString, ARRAYSIZE(aSystemString), "8mosind 0 write %d %s", pinNumber, pinState == 1 ? "on" : "off");
						retval = system(aSystemString);

						if (retval != 0)
						{
							Log("Slot[%d] Failed to write mosfet, returned %d", psParams->slotIndex, retval);
							snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to write mosfet, returned %d", retval);
							strncat(pResp, aBufTemp, LenResp);
						}
						else
						{
							Log("Slot[%d] Mosfet %s (%d) --> %s", psParams->slotIndex, aaToken[1], pinNumber, aaToken[2]);
							snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Mosfet %s (%d) --> %s", aaToken[1], pinNumber, aaToken[2]);
							strncat(pResp, aBufTemp, LenResp);
						}
					}
				}
			break;

			case COM_MOSREAD:
				Log("Slot[%d] Implement function 'mosread' please!", psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Implement function 'mosread' please!");
				strncat(pResp, aBufTemp, LenResp);
			break;

			case COM_MOSCLEAR:
				retval = server_MosfetClear();

				if (retval != 0)
				{
					Log("Slot[%d] Failed to clear mosfets, returned %d", psParams->slotIndex, retval);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to clear mosfets, returned %d", retval);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					Log("Slot[%d] Cleared mosfets", psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Cleared mosfets");
					strncat(pResp, aBufTemp, LenResp);
				}
			break;

			case COM_ECHO:
				randNr = rand() % ARRAYSIZE(gaaEchoes);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "You shouted... you hear '%s'", gaaEchoes[randNr]);
				strncat(pResp, aBufTemp, LenResp);
				Log("Echoed '%s'", pResp);
			break;

			case COM_EXIT:
				psSlotInfo->requestedDisconnect = true;
			break;
		}
	}
	else
	{
		respondCommandUnknown = true;
	}

	// unknown command error message
	if (respondCommandUnknown)
	{
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Unknown command '%s', use '" COMMAND_STRING_HELP "'", aaToken[0]);
		strncat(pResp, aBufTemp, LenResp);
	}
	else if (respondParametersWrong)
	{
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Invalid parameters, use '" COMMAND_STRING_HELP "'");
		strncat(pResp, aBufTemp, LenResp);
	}

	// add newline to buffer
	strncat(pResp, "\n", LenResp);

	// uppercase letters
	//core_StringToUpper(pResp, LenResp);
}

int CServer::IsCommandExecutable(S_SLOTINFO *psSlotInfo, int Flags)
{
	int isExecutable = false;

	// only logged in can execute flag: logged in
	if (psSlotInfo->loggedIn)
	{
		if ((Flags & COMFLAG_EXEC_LOGGEDIN))
			isExecutable = true;
	}
	else// only logged out can execute flag: logged out
	{
		if ((Flags & COMFLAG_EXEC_LOGGEDOUT))
			isExecutable = true;
	}

	// only activated can execute flag: activatedonly
	if (!psSlotInfo->activated)
	{
		if ((Flags & COMFLAG_EXEC_ACTIVATEDONLY))
			isExecutable = false;
	}

	return isExecutable;
}

int CServer::IsCommandVisible(S_SLOTINFO *psSlotInfo, int Flags)
{
	int isVisible = false;

	// only logged in can see flag: logged in
	if (psSlotInfo->loggedIn)
	{
		if ((Flags & COMFLAG_VISI_LOGGEDIN))
			isVisible = true;
	}
	else// only logged out can see flag: logged out
	{
		if ((Flags & COMFLAG_VISI_LOGGEDOUT))
			isVisible = true;
	}

	// only activated can see flag: activatedonly
	if (!psSlotInfo->activated)
	{
		if ((Flags & COMFLAG_VISI_ACTIVATEDONLY))
			isVisible = false;
	}

	return isVisible;
}

int CServer::MosfetClear()
{
	int retval = 0;

	retval = system("8mosind 0 write 0");

	if (retval != 0)
		return ERROR;

	return OK;
}

int CServer::ResetHardware()
{
	int retval = 0;

	// clear GPIO pins
	hardware_Clear();

	// clear mosfets
	retval = server_MosfetClear();
	if (retval != 0)
		Log("%s: Failed to clear mosfets", __FUNCTION__);

	return retval;
}

void CServer::MakeFilepath(E_FILETYPES FileType, const char *pUsername, int FilenameOnly, char *pFullpath, size_t LenFullpath)
{
	// prefix filename with "_" so filename exploits do not work like con/aux/... in windows
	if (FilenameOnly)
	{
		snprintf(pFullpath, LenFullpath, "_%s", pUsername);
		return;
	}
	else
	{
		switch(FileType)
		{
			case FILETYPE_ACCOUNT:
				snprintf(pFullpath, LenFullpath, "%s/_%s", FOLDERPATH_ACCOUNTS, pUsername);
				return;
			break;

			case FILETYPE_DEFINES_GPIO:
				snprintf(pFullpath, LenFullpath, "%s/_%s", FOLDERPATH_DEFINES_GPIO, pUsername);
				return;
			break;

			case FILETYPE_DEFINES_MOSFET:
				snprintf(pFullpath, LenFullpath, "%s/_%s", FOLDERPATH_DEFINES_MOSFET, pUsername);
				return;
			break;

			case FILETYPE_LOG:
				snprintf(pFullpath, LenFullpath, "%s", gsMain.aFilepathLog);
				return;
			break;
		}
	}
}

int CServer::AccountAction(E_ACCOUNTACTIONS AccountAction, S_SLOTINFO *psSlotInfo, const char *pUsername, const char *pPassword, int *pWrongCredentials, char *pError, size_t LenError)
{
	FILE *pFile = 0;
	char aFilename[MAX_LEN_USERFILES] = { 0 };
	char aFilepath[MAX_LEN_FILEPATH] = { 0 };
	char aBufTemp[NET_BUFFERSIZE] = { 0 };
	int len = 0;
	int fileCount = 0;
	int retval = 0;
	char aRead[MAX_LEN_LINES] = { 0 };
	int fileExists = false;

	// reset wrong credentials variable
	if (pWrongCredentials)
		*pWrongCredentials = false;

	// register / login check credentials for validity
	if (AccountAction == ACCACTION_REGISTER || AccountAction == ACCACTION_LOGIN)
	{
		// check username length
		len = strnlen(pUsername, MAX_LEN_USERFILES);
		if (len < MIN_LEN_CREDENTIALS || len > MAX_LEN_CREDENTIALS)
		{
			Log("%s: Username too short/long", __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Username must be " STRINGIFY_VALUE(MIN_LEN_CREDENTIALS) " - " STRINGIFY_VALUE(MAX_LEN_CREDENTIALS) " characters");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check username ascii
		if (!core_CheckStringAscii(pUsername, MAX_LEN_CREDENTIALS + 1))
		{
			Log("%s: Username must only contain the characters " CHARRANGE_ASCII_READABLE, __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Username must only contain the characters " CHARRANGE_ASCII_READABLE);
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check password length
		len = strnlen(pPassword, MAX_LEN_USERFILES);
		if (len < MIN_LEN_CREDENTIALS || len > MAX_LEN_CREDENTIALS)
		{
			Log("%s: Password too short/long", __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Password must be " STRINGIFY_VALUE(MIN_LEN_CREDENTIALS) " - " STRINGIFY_VALUE(MAX_LEN_CREDENTIALS) " characters");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check password ascii
		if (!core_CheckStringAscii(pPassword, MAX_LEN_CREDENTIALS + 1))
		{
			Log("%s: Password must only contain the characters " CHARRANGE_ASCII_READABLE, __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Password must only contain the characters " CHARRANGE_ASCII_READABLE);
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check if account file already existing
		server_MakeFilepath(FILETYPE_ACCOUNT, pUsername, true, aFilename, ARRAYSIZE(aFilename));
		fileExists = core_CheckFileExists(FOLDERPATH_ACCOUNTS, aFilename, ARRAYSIZE(aFilename));
	}

	switch(AccountAction)
	{
		case ACCACTION_REGISTER:
			// if account file already exists
			if (fileExists != false)// Treat ERROR as "file exists"
			{
				Log("%s: File already exists", __FUNCTION__);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "User already exists");
				strncat(pError, aBufTemp, LenError);
				return ERROR;
			}

			// check if any more registrations allowed
			fileCount = core_CountFilesDirectory(FOLDERPATH_ACCOUNTS);
			if (fileCount >= MAX_ACCOUNTS)
			{
				Log("%s: Too many accounts exist %d/%d", __FUNCTION__, fileCount, MAX_ACCOUNTS);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Account limit reached, no more registration allowed");
				strncat(pError, aBufTemp, LenError);
				return ERROR;
			}

			// make filepath
			server_MakeFilepath(FILETYPE_ACCOUNT, pUsername, false, aFilepath, ARRAYSIZE(aFilepath));

			// create file
			pFile = fopen(aFilepath, "w");
			if (!pFile)
			{
				Log("%s: Error opening file for account", __FUNCTION__);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "An error occurred, please try again later");
				strncat(pError, aBufTemp, LenError);
				return ERROR;
			}

			// write account data
			fprintf(pFile, "%s\n", pUsername);
			fprintf(pFile, "%s\n", pPassword);
			fprintf(pFile, "%d\n", 0);// activated

			// close file
			fclose(pFile);

			// create defines file gpio
			retval = server_CreateDefinesFile(FILETYPE_DEFINES_GPIO, pUsername, pError, LenError);
			if (retval != OK)
				return ERROR;

			// create defines file mosfet
			retval = server_CreateDefinesFile(FILETYPE_DEFINES_MOSFET, pUsername, pError, LenError);
			if (retval != OK)
				return ERROR;
		break;

		case ACCACTION_LOGIN:
			// if account file already exists
			if (fileExists != true)
			{
				Log("%s: Username '%s' does not exist", __FUNCTION__, pUsername);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), RESPONSE_LOGIN_FAILED);
				strncat(pError, aBufTemp, LenError);

				if (pWrongCredentials)
					*pWrongCredentials = true;

				return ERROR;
			}

			// compare passwords
			retval = server_ReadKey(pUsername, FILETYPE_ACCOUNT, ACCKEY_PASSWORD, aRead, ARRAYSIZE(aRead), pError, LenError);
			if (retval != OK)
				return ERROR;

			if (strcmp(aRead, pPassword) != 0)
			{
				Log("%s: Password '%s' incorrect", __FUNCTION__, pPassword);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), RESPONSE_LOGIN_FAILED);
				strncat(pError, aBufTemp, LenError);

				if (pWrongCredentials)
					*pWrongCredentials = true;

				return ERROR;
			}

			// fill in username
			strncpy(psSlotInfo->aUsername, pUsername, ARRAYSIZE(psSlotInfo->aUsername));

			// log in
			psSlotInfo->loggedIn = true;

			// activated
			retval = server_ReadKey(pUsername, FILETYPE_ACCOUNT, ACCKEY_ACTIVATED, aRead, ARRAYSIZE(aRead), pError, LenError);
			if (retval != OK)
				return ERROR;

			psSlotInfo->activated = atoi(aRead);

			// reset failed logins
			psSlotInfo->failedLogins = 0;

			// remove IP from suspicious list if the account is activated, else non activated accounts could exploit the counter reset by logging in with a deactivated account
			if (psSlotInfo->activated)
				server_RemoveSuspiciousIP(psSlotInfo->clientIP);
		break;

		case ACCACTION_LOGOUT:
			server_ResetSlot(psSlotInfo);
		break;
	}

	return OK;
}

int CServer::CreateDefinesFile(E_FILETYPES FileType, const char *pUsername, char *pError, size_t LenError)
{
	FILE *pFile = 0;
	char aFilepath[MAX_LEN_FILEPATH] = { 0 };
	char aBufTemp[NET_BUFFERSIZE] = { 0 };
	int i = 0;

	// check file type
	if (FileType != FILETYPE_DEFINES_GPIO && FileType != FILETYPE_DEFINES_MOSFET)
	{
		Log("%s: Error, invalid filetype %d", __FUNCTION__, FileType);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "An error occurred");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	// make filepath
	server_MakeFilepath(FileType, pUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// create file
	pFile = fopen(aFilepath, "w");
	if (!pFile)
	{
		Log("%s: Error opening file for defines", __FUNCTION__);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "An error occurred");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	// write stuff
	for(i = 0; i < MAX_LINES; ++i)
		fprintf(pFile, "\n");

	fclose(pFile);

	return OK;
}

int CServer::WriteKey(S_SLOTINFO *psSlotInfo, E_FILETYPES FileType, int Key, const char *pValue, char *pError, size_t LenError)
{
	FILE *pFile = 0;
	char aFilepath[MAX_LEN_FILEPATH] = { 0 };
	int line = 0;
	char aLine[MAX_LINES][MAX_LEN_LINES] = { { 0 } };
	signed char ch = 0;
	int chCount = 0;
	int lineIndex = 0;
	int bufIndex = 0;
	int i = 0;
	int eofReached = false;
	char aBufTemp[NET_BUFFERSIZE] = { 0 };

	// filepath
	server_MakeFilepath(FileType, psSlotInfo->aUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// read
	pFile = fopen(aFilepath, "r");
	if (!pFile)
	{
		Log("%s: Error opening file for read", __FUNCTION__);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error opening file for read");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	// read all lines
	while(1)
	{
		ch = getc(pFile);

		// safety
		if (chCount >= ARRAYSIZE(aLine) * ARRAYSIZE(aLine[0]) || lineIndex >= ARRAYSIZE(aLine))
		{
			break;
		}

		if (ch == EOF)
		{
			ch = '\0';
			eofReached = true;
		}

		aLine[lineIndex][bufIndex] = ch;
		bufIndex++;

		if (ch == '\n' || eofReached)
		{
			bufIndex = 0;
			lineIndex++;
		}

		if (eofReached)
			break;

		chCount++;
	}

	fclose(pFile);
	line = Key;

	// change value
	snprintf(aLine[line], ARRAYSIZE(aLine[0]), "%s\n", pValue);

	// write back
	pFile = fopen(aFilepath, "w");
	if (!pFile)
	{
		Log("%s: Error opening file for write", __FUNCTION__);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error opening file for write");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	for(i = 0; i < ARRAYSIZE(aLine); ++i)
		fprintf(pFile, aLine[i]);

	fclose(pFile);

	return OK;
}

int CServer::ReadKey(const char *pUsername, E_FILETYPES FileType, int Key, char *pKey, size_t LenKey, char *pError, size_t LenError)
{
	FILE *pFile = 0;
	char aFilepath[MAX_LEN_FILEPATH] = { 0 };
	int line = 0;
	char aLine[MAX_LINES][MAX_LEN_LINES] = { 0 };
	signed char ch = 0;
	int chCount = 0;
	int lineIndex = 0;
	int bufIndex = 0;
	int i = 0;
	int eofReached = false;
	char aBufTemp[NET_BUFFERSIZE] = { 0 };

	// filepath
	server_MakeFilepath(FileType, pUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// read
	pFile = fopen(aFilepath, "r");
	if (!pFile)
	{
		Log("%s: Error opening file for read", __FUNCTION__);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error opening file for read");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	// read all lines
	while(1)
	{
		ch = getc(pFile);

		// safety
		if (chCount >= ARRAYSIZE(aLine) * ARRAYSIZE(aLine[0]))
		{
			break;
		}

		if (ch == EOF)
		{
			ch = '\0';
			eofReached = true;
		}

		aLine[lineIndex][bufIndex] = ch;
		bufIndex++;

		if (ch == '\n' || eofReached)
		{
			if (lineIndex == Key)
			{
				memset(pKey, 0, LenKey);
				core_StringCopyIgnore(pKey, aLine[lineIndex], LenKey, "\n");
				fclose(pFile);
				return OK;
			}

			bufIndex = 0;
			lineIndex++;
		}

		if (eofReached)
			break;

		chCount++;
	}

	fclose(pFile);

	Log("%s: Error, line %d not read", __FUNCTION__, Key);
	snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error reading line %d", Key);
	strncat(pError, aBufTemp, LenError);

	return ERROR;
}

int CServer::RemoveFile(S_SLOTINFO *psSlotInfo, E_FILETYPES FileType, char *pError, size_t LenError)
{
	int retval = 0;
	char aFilepath[MAX_LEN_FILEPATH] = { 0 };
	char aBufTemp[NET_BUFFERSIZE] = { 0 };

	// filepath
	server_MakeFilepath(FileType, psSlotInfo->aUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// remove
	retval = core_RemoveFile(aFilepath);
	if (retval != OK)
	{
		Log("%s: Error removing file '%s'", __FUNCTION__, aFilepath);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error removing file");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	return OK;
}

int CServer::BanIP(in_addr_t IP, time_t Duration)
{
	int banSlotCount = 0;
	int i = 0;
	int amtSlots = ARRAYSIZE(gsServerInfo.aBannedIPs);
	int freeIndex = -1;

	// check if a ban-slot is free
	for(i = 0; i < amtSlots; ++i)
	{
		if (gsServerInfo.aBannedIPs[i] != 0)
			banSlotCount++;
		else if (freeIndex == -1)
			freeIndex = i;
	}

	if (banSlotCount >= amtSlots)
	{
		Log("%s: All banned IP slots occupied (%d)", __FUNCTION__, amtSlots);
		return ERROR;
	}

	// ban IP
	gsServerInfo.aBannedIPs[freeIndex] = IP;
	gsServerInfo.aBanStartTime[freeIndex] = time(0);
	gsServerInfo.aBanDuration[freeIndex] = Duration;

	return OK;
}

int CServer::CheckIPBanned(S_SLOTINFO *psSlotInfo, struct tm *psTime)
{
	int i = 0;
	time_t currentTime = time(0);
	time_t timeRemaining = 0;

	for(i = 0; i < ARRAYSIZE(gsServerInfo.aBannedIPs); ++i)
	{
		if (psSlotInfo->clientIP == gsServerInfo.aBannedIPs[i])
		{
			// calculate remaining ban time
			timeRemaining = (gsServerInfo.aBanStartTime[i] + gsServerInfo.aBanDuration[i]) - currentTime;
			*psTime = *localtime(&timeRemaining);
			return true;
		}
	}

	return false;
}

void CServer::CleanOldSession(S_SLOTINFO *psSlotInfo)
{
	pthread_t clientConnection = psSlotInfo->thrClientConnection;

	// reset slot and close socket to free fd
	server_CloseClientSocket(psSlotInfo);

	// reset slot
	memset(psSlotInfo, 0, sizeof(S_SLOTINFO));

	// cancel unused thread
	pthread_cancel(clientConnection);// do not check return value, the thread could have been cancelled by itself from disconnecting client or banning
}

void CServer::CloseClientSocket(S_SLOTINFO *psSlotInfo)
{
	ResetSlot(psSlotInfo);
	close(psSlotInfo->connfd);
}

void CServer::ResetSlot(S_SLOTINFO *psSlotInfo)
{
	memset(psSlotInfo->aUsername, 0, MAX_LEN_CREDENTIALS + 1);
	psSlotInfo->loggedIn = false;
	psSlotInfo->activated = false;
	psSlotInfo->failedLogins = 0;
	psSlotInfo->banned = false;
}

int CServer::AddSuspiciousIP(in_addr_t IP)
{
	int knownSlotIndex = -1;
	int i = 0;
	int amtSlots = ARRAYSIZE(gsServerInfo.aSuspiciousIPs);
	int addedIP = false;

	// check if the IP is already known
	for(i = 0; i < amtSlots; ++i)
	{
		if (gsServerInfo.aSuspiciousIPs[i] == IP)
		{
			knownSlotIndex = i;
			break;
		}
	}

	// if slot was found, increase the sus counter
	if (knownSlotIndex != -1)
	{
		gsServerInfo.aSuspiciousAttempts[knownSlotIndex]++;
		addedIP = true;

		// refresh suspicious start time
		gsServerInfo.aSuspiciousStartTime[knownSlotIndex] = time(0);
	}
	else// add the IP to the first free slot
	{
		for(i = 0; i < amtSlots; ++i)
		{
			if (gsServerInfo.aSuspiciousIPs[i] == 0)
			{
				gsServerInfo.aSuspiciousIPs[i] = IP;
				gsServerInfo.aSuspiciousAttempts[i]++;
				gsServerInfo.aSuspiciousStartTime[i] = time(0);
				addedIP = true;
				break;
			}
		}
	}

	// if no IP matched and all slots full, error
	if (!addedIP)
	{
		Log("%s: All suspicious IP slots occupied (%d)", __FUNCTION__, amtSlots);
		return ERROR;
	}

	return OK;
}

void CServer::RemoveSuspiciousIP(in_addr_t IP)
{
	int knownSlotIndex = -1;
	int i = 0;
	int amtSlots = ARRAYSIZE(gsServerInfo.aSuspiciousIPs);
	int addedIP = false;

	// check if the IP is already known
	for(i = 0; i < amtSlots; ++i)
	{
		if (gsServerInfo.aSuspiciousIPs[i] == IP)
		{
			gsServerInfo.aSuspiciousIPs[i] = 0;
			gsServerInfo.aSuspiciousAttempts[i] = 0;
			gsServerInfo.aSuspiciousStartTime[i] = 0;

			return;
		}
	}
}

int CServer::GetSuspiciousAttempts(in_addr_t IP)
{
	int i = 0;
	int attempts = 0;

	// check if IP in list
	for(i = 0; i < ARRAYSIZE(gsServerInfo.aSuspiciousIPs); ++i)
	{
		if (IP == gsServerInfo.aSuspiciousIPs[i])
		{
			attempts = gsServerInfo.aSuspiciousAttempts[i];
			break;
		}
	}

	return attempts;
}

struct in_addr CServer::GetIPStruct(in_addr_t IP)
{
	struct in_addr sInAddr = { 0 };

	sInAddr.s_addr = IP;

	return sInAddr;
}