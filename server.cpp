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
#include <sys/select.h>
#include <signal.h>

#include "main.h"
#include "server.h"
#include "mainlogic.h"
#include "core.h"

// global variables
S_COMMAND gasCommands[] =
	{
		{COM_HELP, CSERVER_COMMAND_STRING_HELP, "Lists this help", "", "", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_LOGGEDOUT | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_LOGGEDOUT},
		{COM_ACTIVATEACCOUNT, "i", "Activates your account and unlocks all commands", CSERVER_COM_ACTIVATE_PASS1 " " CSERVER_COM_ACTIVATE_PASS2 " " CSERVER_COM_ACTIVATE_PASS3, "", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN},
		{COM_REGISTER, "register", "Registers an account, " STRINGIFY_VALUE(CSERVER_MIN_LEN_CREDENTIALS) " - " STRINGIFY_VALUE(CSERVER_MAX_LEN_CREDENTIALS) " characters", "<username> <password>", "bob ross", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_LOGGEDOUT | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_LOGGEDOUT},
		{COM_LOGIN, "login", "Logs an account in, " STRINGIFY_VALUE(CSERVER_MIN_LEN_CREDENTIALS) " - " STRINGIFY_VALUE(CSERVER_MAX_LEN_CREDENTIALS) " characters", "<username> <password>", "bob ross", CSERVER_COMFLAG_VISI_LOGGEDOUT | CSERVER_COMFLAG_EXEC_LOGGEDOUT},
		{COM_LOGOUT, "logout", "Logs an account out", "", "", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_EXEC_LOGGEDIN},
		{COM_SHUTDOWN, "shutdown", "Shuts down the server", "", "", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_ACTIVATEDONLY},
		{COM_RUN, "run", "Runs a console command on the machine", "<command>", "reboot", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_ACTIVATEDONLY},
		{COM_DELETE, "delete", "Deletes the program log, your defines, your account or all files", "log/defines/account/all", "log", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_ACTIVATEDONLY},
		{COM_DEFINE_GPIO, "define_gpio", "Defines the name of a GPIO. The name must begin with a letter, and have " STRINGIFY_VALUE(CSERVER_MIN_LEN_DEFINES) " - " STRINGIFY_VALUE(CSERVER_MAX_LEN_DEFINES) " characters. Name is case insensitive", "<GPIO number " CSERVER_COM_GPIO_RANGE "> <name>", "0 fan", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_ACTIVATEDONLY},
		{COM_SET, "set", "Sets the status of a GPIO, all GPIOs are output", "<GPIO number " CSERVER_COM_GPIO_RANGE "/name> <1/0/on/off/high/low>", "fan on", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_ACTIVATEDONLY},
		{COM_CLEAR, "clear", "Clears the status of all GPIOs", "", "", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_ACTIVATEDONLY},
		{COM_DEFINE_MOSFET, "define_mosfet", "Defines the name of a MOSFET. The name must begin with a letter, and have " STRINGIFY_VALUE(CSERVER_MIN_LEN_DEFINES) " - " STRINGIFY_VALUE(CSERVER_MAX_LEN_DEFINES) " characters. Name is case insensitive", "<MOSFET number " CSERVER_COM_MOSFET_RANGE "> <name>", "1 lawnmower", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_ACTIVATEDONLY},
		{COM_MOSSET, "mosset", "Sets the status of a MOSFET", "<MOSFET number " CSERVER_COM_MOSFET_RANGE "/name> <1/0/on/off/high/low>", "lawnmower on", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_ACTIVATEDONLY},
		{COM_MOSREAD, "mosread", "(Not yet implemented!) Reads the status of all MOSFETs", "", "", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_ACTIVATEDONLY},
		{COM_MOSCLEAR, "mosclear", "Clears the status of all MOSFETs", "", "", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_ACTIVATEDONLY | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_ACTIVATEDONLY},
		{COM_ECHO, "echo", "The echo which echoes", "", "", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_LOGGEDOUT | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_LOGGEDOUT},
		{COM_EXIT, "exit", "Closes your connection", "", "", CSERVER_COMFLAG_VISI_LOGGEDIN | CSERVER_COMFLAG_VISI_LOGGEDOUT | CSERVER_COMFLAG_EXEC_LOGGEDIN | CSERVER_COMFLAG_EXEC_LOGGEDOUT},
};

char gaaEchoes[][CSERVER_MAX_LEN_ECHO] =
	{
		{"Ahuurr durr"},
		{"Yodelihuhuu"},
		{"AAAAAAAAAAAH"},
		{"Icy fire"},
		{"Hello"},
		{"Don't you scream at me lil boi"},
		{"Stop"},
		{"I said don't touch me... EEK"},
		{"Meow"},
		{"Bark"},
		{"Echo"},
		{"HADOUKEN"},
		{"Hypopotomonstrosesquipedaliophobia"},
		{"Mosquito bun"},
		{"A: Will you stop permanently loudmouthing me? B: Yesn't"},
		{"A: What's your name? B: Vanessa! A: No your real name! B: Vanessa! No your real name! B: FRAAAANK!"},
		{"Hey... did you hear that?"},
		{"Shush there's something in the bushes!"},
		{"EEEEY I SAID STOP NO SCREAMING. WHY IS IT STILL SO LOUD? Oh wait thats me."},
		{"You sussy baka"},
		{"Yes please?"},
		{"Yarr I'm a Pirate, I am a Pirate"},
};

CServer::CServer(CMainlogic *pMainlogic)
{
	m_pMainlogic = pMainlogic;
	m_sServerInfo = {0};
	memset(&m_asSlotInfo, 0, sizeof(m_asSlotInfo));
}

CServer::~CServer()
{
	CCore::DetachThreadSafely(&m_sServerInfo.thrUpdate);

	for (int i = 0; i < ARRAYSIZE(m_asSlotInfo); ++i)
		CCore::DetachThreadSafely(&m_asSlotInfo[i].thrClientConnection);
}

int CServer::Run()
{
	int retval = 0;
	int connfdCurrent = 0;
	struct sigaction sigAction = {0};
	struct sockaddr_in sockaddr_current = {0};
	socklen_t socklenCurrent = 0;
	struct sockaddr_in serv_addr = {0};
	int i = 0;
	int slotIndex = 0;
	int sockOptValue = 0;
	S_SLOTINFO *psSlotInfo = 0;

	printf("\nwaiting...\n");
	m_pMainlogic->m_pLog->Log("NICE NICE NICE NICE");
	usleep(1000000);
	m_pMainlogic->m_pLog->Log("%s: NICE NICE NICE NICE", __FUNCTION__);
	printf("\nenterint...\n");

	// make process ignore SIGPIPE signal, so it will not exit when writing to disconnected socket
	sigAction.sa_handler = SIG_IGN;
	retval = sigaction(SIGPIPE, &sigAction, NULL);
	if (retval != 0)
	{
		m_pMainlogic->m_pLog->Log("%s: Failed to ignore the SIGPIPE signal", __FUNCTION__);
		m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
		return ERROR;
	}

	// reset hardware
	retval = ResetHardware();
	if (retval != OK)
	{
		m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
		return ERROR;
	}

	// randomize for echo
	srand(time(0));

	// create accounts folder paths
	retval = CCore::Mkdir(CSERVER_FOLDERPATH_ACCOUNTS);
	if (retval != OK)
	{
		m_pMainlogic->m_pLog->Log("%s: Failed to make folder \"%s\"", CSERVER_FOLDERPATH_ACCOUNTS);
		m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
		return ERROR;
	}

	// create defines GPIO folder path
	retval = CCore::Mkdir(CSERVER_FOLDERPATH_DEFINES_GPIO);
	if (retval != OK)
	{
		m_pMainlogic->m_pLog->Log("%s: Failed to make folder \"%s\"", CSERVER_FOLDERPATH_DEFINES_GPIO);
		m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
		return ERROR;
	}

	// create defines MOSFET folder path
	retval = CCore::Mkdir(CSERVER_FOLDERPATH_DEFINES_MOSFET);
	if (retval != OK)
	{
		m_pMainlogic->m_pLog->Log("%s: Failed to make folder \"%s\"", CSERVER_FOLDERPATH_DEFINES_MOSFET);
		m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
		return ERROR;
	}

	// spawn update thread
	m_sServerInfo.thrUpdate = std::thread(&CServer::Update, this);

	// create server socket
	m_sServerInfo.listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_sServerInfo.listenfd == -1)
	{
		m_pMainlogic->m_pLog->Log("%s: Failed to create listenfd, errno: %d", __FUNCTION__, errno);
		m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
		return ERROR;
	}

	// server socket option: allow reusing socket
	sockOptValue = 1;
	retval = setsockopt(m_sServerInfo.listenfd, SOL_SOCKET, SO_REUSEADDR, &sockOptValue, sizeof(sockOptValue));
	if (retval != 0)
	{
		m_pMainlogic->m_pLog->Log("%s: Failed to set socket option: SO_REUSEADDR, errno: %d", __FUNCTION__, errno);
		m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
		return ERROR;
	}

	// set up server settings
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(CSERVER_NET_PORT);

	// bind server socket with settings
	retval = bind(m_sServerInfo.listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (retval != 0)
	{
		m_pMainlogic->m_pLog->Log("%s: Failed to bind, errno: %d", __FUNCTION__, errno);
		m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
		return ERROR;
	}

	// start listening
	retval = listen(m_sServerInfo.listenfd, CSERVER_MAX_LEN_LISTENQUEUE);
	if (retval != 0)
	{
		m_pMainlogic->m_pLog->Log("%s: Failed to listen, errno: %d", __FUNCTION__, errno);
		m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
		return ERROR;
	}

	// server loop
	m_pMainlogic->m_pLog->Log("Server running...");

	while (1)
	{
		// accept incoming connections
		m_pMainlogic->m_pLog->Log("Ready for new connection...");

		socklenCurrent = sizeof(struct sockaddr);
		connfdCurrent = accept(m_sServerInfo.listenfd, (struct sockaddr *)&sockaddr_current, &socklenCurrent);

		// check connection details
		m_pMainlogic->m_pLog->Log("Accepted port %d, IP %s", htons(sockaddr_current.sin_port), inet_ntoa(sockaddr_current.sin_addr));

		// check if a client with the same IP has been connected before
		slotIndex = -1;

		for (i = 0; i < ARRAYSIZE(m_asSlotInfo); ++i)
		{
			if (sockaddr_current.sin_addr.s_addr == m_asSlotInfo[i].clientIP)
			{
				slotIndex = i;
				break;
			}
		}

		// same IP, clean up old thread
		if (slotIndex > -1)
		{
			m_pMainlogic->m_pLog->Log("Slot[%d] Same IP detected, cleaning up...", slotIndex);
			CleanOldSession(&m_asSlotInfo[slotIndex]);
		}

		// find first free client index
		slotIndex = -1;

		for (i = 0; i < ARRAYSIZE(m_asSlotInfo); ++i)
		{
			if (m_asSlotInfo[i].clientIP == 0)
			{
				slotIndex = i;
				break;
			}
		}

		// if no free client index was found
		if (slotIndex < 0)
		{
			m_pMainlogic->m_pLog->Log("Max client connections reached, no thread created");
			close(connfdCurrent);
			continue;
		}

		// client socket option: enable keepalive
		sockOptValue = 1;
		retval = setsockopt(connfdCurrent, SOL_SOCKET, SO_KEEPALIVE, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			m_pMainlogic->m_pLog->Log("%s: Failed to set socket option: SO_KEEPALIVE, errno: %d", __FUNCTION__, errno);
			close(connfdCurrent);
			m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
			return ERROR;
		}

		// client socket option: keepalive idle time
		sockOptValue = 10; // s
		retval = setsockopt(connfdCurrent, IPPROTO_TCP, TCP_KEEPIDLE, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			m_pMainlogic->m_pLog->Log("%s: Failed to set socket option: TCP_KEEPIDLE, errno: %d", __FUNCTION__, errno);
			close(connfdCurrent);
			m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
			return ERROR;
		}

		// client socket option: keepalive interval
		sockOptValue = 10; // s
		retval = setsockopt(connfdCurrent, IPPROTO_TCP, TCP_KEEPINTVL, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			m_pMainlogic->m_pLog->Log("%s: Failed to set socket option: TCP_KEEPINTVL, errno: %d", __FUNCTION__, errno);
			close(connfdCurrent);
			m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
			return ERROR;
		}

		// client socket option: keepalive probes
		sockOptValue = 2;
		retval = setsockopt(connfdCurrent, IPPROTO_TCP, TCP_KEEPCNT, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			m_pMainlogic->m_pLog->Log("%s: Failed to set socket option: TCP_KEEPCNT, errno: %d", __FUNCTION__, errno);
			close(connfdCurrent);
			m_pMainlogic->SetThreadStatus(THR_SERVERRUN, ERROR);
			return ERROR;
		}

		psSlotInfo = &m_asSlotInfo[slotIndex];

		// add IP to list
		psSlotInfo->clientIP = sockaddr_current.sin_addr.s_addr;

		// assign connection socket
		psSlotInfo->connfd = connfdCurrent;

		// create thread
		m_pMainlogic->m_pLog->Log("Slot[%d] Creating new thread...", slotIndex);

		psSlotInfo->threadParamsClientConnection.sockfd = psSlotInfo->connfd;
		psSlotInfo->threadParamsClientConnection.slotIndex = slotIndex;
		psSlotInfo->thrClientConnection = std::thread(&CServer::ClientConnection, this, &psSlotInfo->threadParamsClientConnection);
	}

	return OK;
}

int CServer::ClientConnection(S_PARAMS_CLIENTCONNECTION *psParams)
{
	int retval = 0;
	char aBufRaw[CSERVER_NET_BUFFERSIZE] = {0};
	char aBuf[CSERVER_NET_BUFFERSIZE] = {0};
	char aBufResp[CSERVER_NET_BUFFERSIZE] = {0};
	S_SLOTINFO *psSlotInfo = &m_asSlotInfo[psParams->slotIndex];
	struct tm sTime = {0};
	struct timeval sTimeval = {0};
	fd_set sFDSet = {0};
	int timeoutOccurred = false;

	// check if IP banned, then close socket
	if (CheckIPBanned(psSlotInfo, &sTime))
	{
		m_pMainlogic->m_pLog->Log("Slot[%d] IP '%s' still banned for %dh %dm %ds, cleaning up...", psParams->slotIndex, inet_ntoa(GetIPStruct(psSlotInfo->clientIP)), sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
		snprintf(aBufResp, ARRAYSIZE(aBufResp), CSERVER_RESPONSE_PREFIX "You are banned for %dh %dm %ds\n", sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
		write(psParams->sockfd, aBufResp, ARRAYSIZE(aBufResp));
		CleanOldSession(psSlotInfo);
		return ERROR;
	}

	// read from client loop
	while (1)
	{
		// write response prefix to client
		if (!timeoutOccurred)
			write(psParams->sockfd, CSERVER_RESPONSE_PREFIX, ARRAYSIZE(CSERVER_RESPONSE_PREFIX));

		// add server file descriptor to select watch list
		FD_ZERO(&sFDSet);
		FD_SET(psParams->sockfd, &sFDSet);

		// set select timeout
		memset(&sTimeval, 0, sizeof(sTimeval));
		sTimeval.tv_sec = CSERVER_TIMEOUT_CLIENTCONNECTION_READ;

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
			CCore::StringCopyIgnore(aBuf, aBufRaw, ARRAYSIZE(aBuf), "\r\n");

			// check if client disconnected and close socket
			if (strnlen(aBufRaw, ARRAYSIZE(aBuf)) == 0)
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Disconnected", psParams->slotIndex);

				CleanOldSession(psSlotInfo);
				return ERROR;
			}
			else if (strnlen(aBuf, ARRAYSIZE(aBuf)) > 0) // client sent message
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] <%s>", psParams->slotIndex, aBuf);

				memset(aBufResp, 0, ARRAYSIZE(aBufResp));
				retval = ParseMessage(psParams, aBuf, aBufResp, ARRAYSIZE(aBufResp));
				if (retval != OK)
				{
					m_pMainlogic->m_pLog->Log("%s: Slot[%d] Error parsing message", __FUNCTION__, psParams->slotIndex);
				}

				// send response
				if (strlen(aBufResp) > 0)
					write(psParams->sockfd, aBufResp, ARRAYSIZE(aBufResp));

				// check if slot requested disconnect or has been banned and close socket
				if (psSlotInfo->requestedDisconnect || psSlotInfo->banned)
				{
					CleanOldSession(psSlotInfo);
					return ERROR;
				}

				usleep(CSERVER_DELAY_RECEIVE);
			}
		}
		else if (retval == 0) // timeout
		{
			timeoutOccurred = true;
		}
		else // error
		{
			m_pMainlogic->m_pLog->Log("Slot[%d] Error for select(), errno: %d\n", psParams->slotIndex, errno);
		}
	}

	return OK;
}

int CServer::Update()
{
	time_t currentTime = 0;
	int i = 0;

	while (1)
	{
		// get current time
		currentTime = time(0);

		// update IP bans
		for (i = 0; i < ARRAYSIZE(m_sServerInfo.aBannedIPs); ++i)
		{
			if (m_sServerInfo.aBannedIPs[i] == 0)
				continue;

			if (currentTime >= m_sServerInfo.aBanStartTime[i] + m_sServerInfo.aBanDuration[i])
			{
				// remove IP from suspicious list
				RemoveSuspiciousIP(m_sServerInfo.aBannedIPs[i]);

				// unban IP
				m_pMainlogic->m_pLog->Log("IP[%d] '%s' has been unbanned", i, inet_ntoa(GetIPStruct(m_sServerInfo.aBannedIPs[i])));
				m_sServerInfo.aBannedIPs[i] = 0;
				m_sServerInfo.aBanStartTime[i] = 0;
				m_sServerInfo.aBanDuration[i] = 0;
			}
		}

		// update suspicious IPs list
		for (i = 0; i < ARRAYSIZE(m_sServerInfo.aSuspiciousIPs); ++i)
		{
			if (m_sServerInfo.aSuspiciousIPs[i] == 0)
				continue;

			if (currentTime >= m_sServerInfo.aSuspiciousStartTime[i] + CSERVER_SUSPICIOUS_FALLOFF_TIME)
			{
				m_pMainlogic->m_pLog->Log("Unlisted suspicious IP[%d] '%s'", i, inet_ntoa(GetIPStruct(m_sServerInfo.aSuspiciousIPs[i])));
				RemoveSuspiciousIP(m_sServerInfo.aSuspiciousIPs[i]);
			}
		}

		usleep(CSERVER_DELAY_UPDATELOOP);
	}
}

int CServer::OnExitApplication()
{
	int i = 0;
	int retval = 0;
	int hasError = false;

	retval = ResetHardware();
	if (retval != OK)
		hasError = true;

	// detach update thread
	retval = CCore::DetachThreadSafely(&m_sServerInfo.thrUpdate);
	if (retval != OK)
		m_pMainlogic->m_pLog->Log("Failed to detach update thread");

	// cancel client connection threads and close sockets
	for (i = 0; i < ARRAYSIZE(m_asSlotInfo); ++i)
	{
		retval = CCore::DetachThreadSafely(&m_asSlotInfo[i].thrClientConnection);
		if (retval != OK)
			m_pMainlogic->m_pLog->Log("Failed to detach client connection thread");

		CloseClientSocket(&m_asSlotInfo[i]);
	}

	// close server socket
	close(m_sServerInfo.listenfd);

	if (hasError)
		return ERROR;

	return OK;
}

int CServer::ParseMessage(S_PARAMS_CLIENTCONNECTION *psParams, const char *pMsg, char *pResp, size_t LenResp)
{
	int retval = 0;
	char *pToken = 0;
	char *pRest = 0;
	char aaToken[CSERVER_MAX_TOKENS][CSERVER_MAX_LEN_TOKEN] = {{0}};
	char aMessage[CSERVER_NET_BUFFERSIZE] = {0};
	int tokenIndex = 0;

	strncpy(aMessage, pMsg, ARRAYSIZE(aMessage));
	pRest = aMessage;

	while (1)
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
	EvaluateTokens(psParams, aaToken, pResp, LenResp, pMsg);

	return OK;
}

void CServer::EvaluateTokens(S_PARAMS_CLIENTCONNECTION *psParams, char aaToken[CSERVER_MAX_TOKENS][CSERVER_MAX_LEN_TOKEN], char *pResp, size_t LenResp, const char *pMsgFull)
{
	int retval = 0;
	int foundMatch = false;
	int commandIndex = 0;
	char aBufTemp[CSERVER_NET_BUFFERSIZE] = {0};
	int randNr = 0;
	S_SLOTINFO *psSlotInfo = &m_asSlotInfo[psParams->slotIndex];
	int commandExecutable = false;
	int commandVisible = false;
	int commandCorrect = false;
	int respondCommandUnknown = false;
	int respondParametersWrong = false;
	int i = 0;
	char aRead[CSERVER_MAX_LEN_LINES] = {0};
	int pinNumber = 0;
	int pinState = 0;
	int isName = 0;
	int wrongCredentials = false;
	int doBan = false;
	time_t banTime = 0;
	struct tm sTime = {0};
	int susAttempts = 0;
	char aFullMessage[CSERVER_NET_BUFFERSIZE] = {0};
	char aFullParameters[CSERVER_NET_BUFFERSIZE] = {0};
	char *pRest = 0;

	// look for command
	for (i = 0; i < ARRAYSIZE(gasCommands); ++i)
	{
		if (CCore::StringCompareNocase(aaToken[0], gasCommands[i].aName, ARRAYSIZE(aaToken[0])) == 0)
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
	strncpy(pResp, CSERVER_RESPONSE_PREFIX, LenResp);

	// command was found
	if (foundMatch)
	{
		// check if command is executable
		commandExecutable = IsCommandExecutable(psSlotInfo, gasCommands[commandIndex].flags);

		// check if command is visible
		commandVisible = IsCommandVisible(psSlotInfo, gasCommands[commandIndex].flags);
	}

	if (commandExecutable)
	{
		m_pMainlogic->m_pLog->Log("Slot[%d] --> '%s'", psParams->slotIndex, gasCommands[commandIndex].aName);

		switch (gasCommands[commandIndex].ID)
		{
		case COM_HELP:
			strncat(pResp, "****** Help ******\n" CSERVER_RESPONSE_PREFIX "All commands are case insensitive.\n", LenResp);

			// if logged in, inform as who
			if (psSlotInfo->loggedIn)
			{
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), CSERVER_RESPONSE_PREFIX "Logged in as '%s'.\n", psSlotInfo->aUsername);
				strncat(pResp, aBufTemp, LenResp);
			}

			// if account activated, inform
			if (psSlotInfo->activated)
			{
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), CSERVER_RESPONSE_PREFIX "Account is activated.\n" CSERVER_RESPONSE_PREFIX "\n");
				strncat(pResp, aBufTemp, LenResp);
			}
			else
			{
				strncat(pResp, CSERVER_RESPONSE_PREFIX "\n", LenResp);
			}

			for (int i = 0; i < ARRAYSIZE(gasCommands); ++i)
			{
				if (!IsCommandVisible(psSlotInfo, gasCommands[i].flags))
					continue;

				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), CSERVER_RESPONSE_PREFIX "%s... %s. Usage: '%s%s%s'.", gasCommands[i].aName, gasCommands[i].aHelp, gasCommands[i].aName, strnlen(gasCommands[i].aParams, ARRAYSIZE(gasCommands[0].aParams)) > 0 ? " " : "", gasCommands[i].aParams);
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

			strncat(pResp, CSERVER_RESPONSE_PREFIX "******************", LenResp);
			break;

		case COM_ACTIVATEACCOUNT:
			// check if the passphrase is correct
			if (CCore::StringCompareNocase(aaToken[1], CSERVER_COM_ACTIVATE_PASS1, ARRAYSIZE(aaToken[0])) == 0 &&
				CCore::StringCompareNocase(aaToken[2], CSERVER_COM_ACTIVATE_PASS2, ARRAYSIZE(aaToken[0])) == 0 &&
				CCore::StringCompareNocase(aaToken[3], CSERVER_COM_ACTIVATE_PASS3, ARRAYSIZE(aaToken[0])) == 0)
				commandCorrect = true;

			if (commandCorrect)
			{
				retval = WriteKey(psSlotInfo, FILETYPE_ACCOUNT, ACCKEY_ACTIVATED, "1", pResp, LenResp);
				if (retval != OK)
				{
					m_pMainlogic->m_pLog->Log("Slot[%d] Failed to activate account '%s'", psParams->slotIndex, psSlotInfo->aUsername);
				}
				else
				{
					psSlotInfo->activated = true;
					strncat(pResp, CSERVER_COM_ACTIVATE_RESPONSE "! Account activated!", LenResp);
					m_pMainlogic->m_pLog->Log("Slot[%d] Account '%s' has been activated", psParams->slotIndex, psSlotInfo->aUsername);
				}
			}
			else
			{
				respondCommandUnknown = true;
			}
			break;

		case COM_REGISTER:
			retval = AccountAction(ACCACTION_REGISTER, psSlotInfo, aaToken[1], aaToken[2], NULL, pResp, LenResp);
			if (retval != OK)
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Failed to register <%s> <%s>", psParams->slotIndex, aaToken[1], aaToken[2]);
			}
			else
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Registered user <%s> <%s>", psParams->slotIndex, aaToken[1], aaToken[2]);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Successfully registered, please log in!");
				strncat(pResp, aBufTemp, LenResp);
			}
			break;

		case COM_LOGIN:
			retval = AccountAction(ACCACTION_LOGIN, psSlotInfo, aaToken[1], aaToken[2], &wrongCredentials, pResp, LenResp);
			if (retval != OK)
			{
				if (wrongCredentials) // if the reason for fail is "wrong credentials"
				{
					psSlotInfo->failedLogins++;
					retval = AddSuspiciousIP(psSlotInfo->clientIP);
					if (retval != OK)
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] Unable to add suspicious IP '%s', safety mesure: Exit application", psParams->slotIndex, inet_ntoa(GetIPStruct(psSlotInfo->clientIP)));
						m_pMainlogic->RequestApplicationExit();
					}
				}

				susAttempts = GetSuspiciousAttempts(psSlotInfo->clientIP);
				m_pMainlogic->m_pLog->Log("Slot[%d] Failed to log in with '%s' '%s', failed logins: %d/%d, total attempts of '%s': %d/%d", psParams->slotIndex, aaToken[1], aaToken[2], psSlotInfo->failedLogins, CSERVER_MAX_FAILED_LOGINS, inet_ntoa(GetIPStruct(psSlotInfo->clientIP)), susAttempts, CSERVER_MAX_FAILED_LOGINS_SUSPICIOUS);

				// check if IP ban necessary
				if (susAttempts >= CSERVER_MAX_FAILED_LOGINS_SUSPICIOUS)
				{
					doBan = true;
					banTime = CSERVER_IP_BAN_DURATION_SUSPICIOUS;
				}
				else if (psSlotInfo->failedLogins >= CSERVER_MAX_FAILED_LOGINS)
				{
					doBan = true;
					banTime = CSERVER_IP_BAN_DURATION_FAILEDLOGINS;
				}

				// check if IP ban necessary by failed attempts
				if (doBan)
				{
					retval = BanIP(psSlotInfo->clientIP, banTime);
					if (retval != OK)
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] Unable to ban IP '%s', safety mesure: Exit application", psParams->slotIndex, inet_ntoa(GetIPStruct(psSlotInfo->clientIP)));
						m_pMainlogic->RequestApplicationExit();
					}
					else
					{
						psSlotInfo->banned = true;
						sTime = *localtime(&banTime);
						m_pMainlogic->m_pLog->Log("Slot[%d] Banned IP '%s' for %dh %dm %ds", psParams->slotIndex, inet_ntoa(GetIPStruct(psSlotInfo->clientIP)), sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "\n" CSERVER_RESPONSE_PREFIX "You have been banned for %dh %dm %ds", sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
						strncat(pResp, aBufTemp, LenResp);
					}
				}
			}
			else
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Account '%s' logged in", psParams->slotIndex, psSlotInfo->aUsername);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Welcome back %s", aaToken[1]);
				strncat(pResp, aBufTemp, LenResp);
			}
			break;

		case COM_LOGOUT:
			retval = AccountAction(ACCACTION_LOGOUT, psSlotInfo, NULL, NULL, NULL, pResp, LenResp);
			if (retval != OK)
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Failed to log out with account '%s'", psParams->slotIndex, psSlotInfo->aUsername);
			}
			else
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Account logged out", psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Logged out");
				strncat(pResp, aBufTemp, LenResp);
			}
			break;

		case COM_SHUTDOWN:
			m_pMainlogic->m_pLog->Log("Slot[%d] Has shut down the server", psParams->slotIndex);
			m_pMainlogic->RequestApplicationExit();
			break;

		case COM_RUN:
			// remove command from parameters
			strncpy(aFullMessage, pMsgFull, ARRAYSIZE(aFullMessage));
			pRest = aFullMessage;
			strtok_r(pRest, " ", &pRest);
			strncpy(aFullParameters, pRest, ARRAYSIZE(aFullParameters));

			m_pMainlogic->m_pLog->Log("Slot[%d] Running command '%s'...", psParams->slotIndex, aFullParameters);

			retval = system(aFullParameters);

			m_pMainlogic->m_pLog->Log("Slot[%d] Command returned %d", psParams->slotIndex, retval);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "'%s' returned %d", aFullParameters, retval);
			strncat(pResp, aBufTemp, LenResp);
			break;

		case COM_DELETE:
			// log
			if (CCore::StringCompareNocase(aaToken[1], "log", ARRAYSIZE(aaToken[0])) == 0)
			{
				retval = RemoveFile(psSlotInfo, FILETYPE_LOG, pResp, LenResp);
				if (retval != OK)
				{
					m_pMainlogic->m_pLog->Log("%s: Failed to remove log file", __FUNCTION__);
				}
				else
				{
					strncat(pResp, "Deleted log file", LenResp);
					m_pMainlogic->m_pLog->Log("Slot[%d] User '%s' deleted log", psParams->slotIndex, psSlotInfo->aUsername);
				}
			} // account
			else if (CCore::StringCompareNocase(aaToken[1], "account", ARRAYSIZE(aaToken[0])) == 0)
			{
				// delete account
				retval = RemoveFile(psSlotInfo, FILETYPE_ACCOUNT, pResp, LenResp);
				if (retval != OK)
				{
					m_pMainlogic->m_pLog->Log("%s: Failed to delete account file, defines were not removed", __FUNCTION__);
				}
				else
				{
					// delete defines GPIO
					retval = RemoveFile(psSlotInfo, FILETYPE_DEFINES_GPIO, pResp, LenResp);
					if (retval != OK)
					{
						m_pMainlogic->m_pLog->Log("%s: Failed to additionally delete defines GPIO file", __FUNCTION__);
					}
					else
					{
						// delete defines MOSFET
						retval = RemoveFile(psSlotInfo, FILETYPE_DEFINES_MOSFET, pResp, LenResp);
						if (retval != OK)
						{
							m_pMainlogic->m_pLog->Log("%s: Failed to additionally delete defines MOSFET file", __FUNCTION__);
						}
						else
						{
							// log out
							retval = AccountAction(ACCACTION_LOGOUT, psSlotInfo, NULL, NULL, NULL, pResp, LenResp);
							if (retval != OK)
							{
								m_pMainlogic->m_pLog->Log("Slot[%d] Failed to additionally log out with account '%s'", psParams->slotIndex, psSlotInfo->aUsername);
							}
							else
							{
								m_pMainlogic->m_pLog->Log("Slot[%d] Deleted account and logged out", psParams->slotIndex);
								snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Deleted account and logged out");
								strncat(pResp, aBufTemp, LenResp);
							}
						}
					}
				}
			} // defines are just cleared because the file is still needed
			else if (CCore::StringCompareNocase(aaToken[1], "defines", ARRAYSIZE(aaToken[0])) == 0)
			{
				// clear defines GPIO
				retval = CreateDefinesFile(FILETYPE_DEFINES_GPIO, psSlotInfo->aUsername, pResp, LenResp);
				if (retval != OK)
				{
					m_pMainlogic->m_pLog->Log("Slot[%d] User '%s' unable to clear defines GPIO", psParams->slotIndex, psSlotInfo->aUsername);
				}
				else
				{
					// clear defines MOSFET
					retval = CreateDefinesFile(FILETYPE_DEFINES_MOSFET, psSlotInfo->aUsername, pResp, LenResp);
					if (retval != OK)
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] User '%s' unable to clear defines MOSFET", psParams->slotIndex, psSlotInfo->aUsername);
					}
					else
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] User '%s' defines were cleared", psParams->slotIndex, psSlotInfo->aUsername);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Deleted defines");
						strncat(pResp, aBufTemp, LenResp);
					}
				}
			} // all
			else if (CCore::StringCompareNocase(aaToken[1], "all", ARRAYSIZE(aaToken[0])) == 0)
			{
				// safety measure in case we ever change the accounts / defines directory to anywhere that is not our program directory
				if (strstr(CSERVER_FOLDERPATH_ACCOUNTS, FILEPATH_BASE) == 0 || strstr(CSERVER_FOLDERPATH_DEFINES_GPIO, FILEPATH_BASE) == 0 || strstr(CSERVER_FOLDERPATH_DEFINES_MOSFET, FILEPATH_BASE) == 0)
				{
					m_pMainlogic->m_pLog->Log("Slot[%d] Danger! Folder path for accounts or defines are not inside the base folder path '%s', important files could be deleted", psParams->slotIndex, FILEPATH_BASE);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Unable to delete all files", CSERVER_FOLDERPATH_ACCOUNTS);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					// remove accounts
					retval = CCore::RemoveFilesDirectory(CSERVER_FOLDERPATH_ACCOUNTS);
					if (retval != OK)
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] Failed to remove files in directory '%s'", psParams->slotIndex, CSERVER_FOLDERPATH_ACCOUNTS);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to remove files in directory '%s'", CSERVER_FOLDERPATH_ACCOUNTS);
						strncat(pResp, aBufTemp, LenResp);
					}
					else
					{
						// remove defines GPIO
						retval = CCore::RemoveFilesDirectory(CSERVER_FOLDERPATH_DEFINES_GPIO);
						if (retval != OK)
						{
							m_pMainlogic->m_pLog->Log("Slot[%d] Failed to remove files in directory '%s'", psParams->slotIndex, CSERVER_FOLDERPATH_DEFINES_GPIO);
							snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to remove files in directory '%s'", CSERVER_FOLDERPATH_DEFINES_GPIO);
							strncat(pResp, aBufTemp, LenResp);
						}
						else
						{
							// remove defines MOSFET
							retval = CCore::RemoveFilesDirectory(CSERVER_FOLDERPATH_DEFINES_MOSFET);
							if (retval != OK)
							{
								m_pMainlogic->m_pLog->Log("Slot[%d] Failed to remove files in directory '%s'", psParams->slotIndex, CSERVER_FOLDERPATH_DEFINES_GPIO);
								snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to remove files in directory '%s'", CSERVER_FOLDERPATH_DEFINES_GPIO);
								strncat(pResp, aBufTemp, LenResp);
							}
							else
							{
								// remove log
								retval = RemoveFile(psSlotInfo, FILETYPE_LOG, pResp, LenResp);
								if (retval != OK)
								{
									m_pMainlogic->m_pLog->Log("Slot[%d] Failed to remove log file", psParams->slotIndex);
								}
								else
								{
									// log out
									retval = AccountAction(ACCACTION_LOGOUT, psSlotInfo, NULL, NULL, NULL, pResp, LenResp);
									if (retval != OK)
									{
										m_pMainlogic->m_pLog->Log("Slot[%d] Failed to additionally log out with account '%s'", psParams->slotIndex, psSlotInfo->aUsername);
									}
									else
									{
										m_pMainlogic->m_pLog->Log("Slot[%d] Deleted all files and logged out", psParams->slotIndex);
										snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Deleted all files and logged out");
										strncat(pResp, aBufTemp, LenResp);
									}
								}
							}
						}
					}
				}
			}
			else // invalid category
			{
				respondParametersWrong = true;
				m_pMainlogic->m_pLog->Log("Slot[%d] Wrong parameters for deleting", psParams->slotIndex);
			}
			break;

		case COM_DEFINE_GPIO:
			pinNumber = atoi(aaToken[1]);

			if (CCore::IsLetter(aaToken[1][0]))
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Error, GPIO number must be a number", psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, GPIO number must be a number");
				strncat(pResp, aBufTemp, LenResp);
			} // number must be in range
			else if (!m_Hardware.IsGpioValid(pinNumber))
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Error, GPIO number must be " CSERVER_COM_GPIO_RANGE, psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, GPIO number must be " CSERVER_COM_GPIO_RANGE);
				strncat(pResp, aBufTemp, LenResp);
			} // name must begin with letter
			else if (!CCore::IsLetter(aaToken[2][0]))
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Error, name must begin with a letter", psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name must begin with a letter");
				strncat(pResp, aBufTemp, LenResp);
			} // name must be simple ascii
			else if (!CCore::CheckStringAscii(aaToken[2], ARRAYSIZE(aaToken[0])))
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Error, name must contain only characters " CSERVER_CHARRANGE_ASCII_READABLE, psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name must contain only characters " CSERVER_CHARRANGE_ASCII_READABLE);
				strncat(pResp, aBufTemp, LenResp);
			}
			else
			{
				if (strlen(aaToken[2]) < CSERVER_MIN_LEN_DEFINES || strlen(aaToken[2]) > CSERVER_MAX_LEN_DEFINES)
				{
					m_pMainlogic->m_pLog->Log("Slot[%d] Error, define name must have %d - %d characters", psParams->slotIndex, CSERVER_MIN_LEN_DEFINES, CSERVER_MAX_LEN_DEFINES);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, define name must have %d - %d characters", CSERVER_MIN_LEN_DEFINES, CSERVER_MAX_LEN_DEFINES);
					strncat(pResp, aBufTemp, LenResp);
				}
				else if (!CCore::CheckStringAscii(aaToken[2], ARRAYSIZE(aaToken[2])))
				{
					m_pMainlogic->m_pLog->Log("Slot[%d] Error, define name must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE, psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, define name must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					retval = WriteKey(psSlotInfo, FILETYPE_DEFINES_GPIO, pinNumber, aaToken[2], pResp, LenResp);
					if (retval != OK)
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] Failed to define <%d> <%s>", psParams->slotIndex, pinNumber, aaToken[2]);
					}
					else
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] Defined GPIO %d as '%s'", psParams->slotIndex, pinNumber, aaToken[2]);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Defined GPIO %d as '%s'", pinNumber, aaToken[2]);
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
			if (CCore::IsLetter(aaToken[1][0]))
				isName = true;

			if (isName)
			{
				for (i = 0; i <= 31; ++i)
				{
					// skip non-GPIO and i2c-1 pins
					if (!m_Hardware.IsGpioValid(i))
						continue;

					retval = ReadKey(psSlotInfo->aUsername, FILETYPE_DEFINES_GPIO, i, aRead, ARRAYSIZE(aRead), pResp, LenResp);
					if (retval != OK)
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] Failed to read key %d", psParams->slotIndex, i);
					}
					else // compare name
					{
						if (CCore::StringCompareNocase(aRead, aaToken[1], ARRAYSIZE(aRead)) == 0)
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

			// check GPIO number
			if (!m_Hardware.IsGpioValid(pinNumber))
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Error, name not found or GPIO number outside " CSERVER_COM_GPIO_RANGE, psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name not found or GPIO number outside " CSERVER_COM_GPIO_RANGE);
				strncat(pResp, aBufTemp, LenResp);
			}
			else
			{
				// check state
				isName = false;
				pinState = -1;

				// check if state is name or number (or empty)
				if (CCore::IsLetter(aaToken[2][0]) || strlen(aaToken[2]) == 0)
					isName = true;

				// check on/off/high/low
				if (isName)
				{
					if (CCore::StringCompareNocase(aaToken[2], "on", ARRAYSIZE(aaToken[2])) == 0)
						pinState = 1;
					else if (CCore::StringCompareNocase(aaToken[2], "off", ARRAYSIZE(aaToken[2])) == 0)
						pinState = 0;
					else if (CCore::StringCompareNocase(aaToken[2], "high", ARRAYSIZE(aaToken[2])) == 0)
						pinState = 1;
					else if (CCore::StringCompareNocase(aaToken[2], "low", ARRAYSIZE(aaToken[2])) == 0)
						pinState = 0;
				}
				else
				{
					pinState = atoi(aaToken[2]);
				}

				// check GPIO state validity
				if (pinState < 0 || pinState > 1)
				{
					m_pMainlogic->m_pLog->Log("Slot[%d] Error unknown GPIO state '%s'", psParams->slotIndex, aaToken[2]);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error unknown GPIO state '%s'", aaToken[2]);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					// set GPIO
					m_Hardware.SetGpio(pinNumber, pinState);

					m_pMainlogic->m_pLog->Log("Slot[%d] GPIO %s (%d) --> %s", psParams->slotIndex, aaToken[1], pinNumber, aaToken[2]);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "GPIO %s (%d) --> %s", aaToken[1], pinNumber, aaToken[2]);
					strncat(pResp, aBufTemp, LenResp);
				}
			}
			break;

		case COM_CLEAR:
			m_Hardware.ClearGpio();

			m_pMainlogic->m_pLog->Log("Slot[%d] Cleared GPIOs", psParams->slotIndex);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Cleared GPIOs");
			strncat(pResp, aBufTemp, LenResp);
			break;

		case COM_DEFINE_MOSFET:
			pinNumber = atoi(aaToken[1]);

			if (CCore::IsLetter(aaToken[1][0]))
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Error, MOSFET number must be a number", psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, MOSFET number must be a number");
				strncat(pResp, aBufTemp, LenResp);
			} // number must be in range
			else if (!m_Hardware.IsMosfetValid(pinNumber))
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Error, MOSFET number must be " CSERVER_COM_MOSFET_RANGE, psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, MOSFET number must be " CSERVER_COM_MOSFET_RANGE);
				strncat(pResp, aBufTemp, LenResp);
			} // name must begin with letter
			else if (!CCore::IsLetter(aaToken[2][0]))
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Error, name must begin with a letter", psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name must begin with a letter");
				strncat(pResp, aBufTemp, LenResp);
			} // name must be simple ascii
			else if (!CCore::CheckStringAscii(aaToken[2], ARRAYSIZE(aaToken[0])))
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Error, name must contain only characters " CSERVER_CHARRANGE_ASCII_READABLE, psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name must contain only characters " CSERVER_CHARRANGE_ASCII_READABLE);
				strncat(pResp, aBufTemp, LenResp);
			}
			else
			{
				if (strlen(aaToken[2]) < CSERVER_MIN_LEN_DEFINES || strlen(aaToken[2]) > CSERVER_MAX_LEN_DEFINES)
				{
					m_pMainlogic->m_pLog->Log("Slot[%d] Error, define name must have %d - %d characters", psParams->slotIndex, CSERVER_MIN_LEN_DEFINES, CSERVER_MAX_LEN_DEFINES);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, define name must have %d - %d characters", CSERVER_MIN_LEN_DEFINES, CSERVER_MAX_LEN_DEFINES);
					strncat(pResp, aBufTemp, LenResp);
				}
				else if (!CCore::CheckStringAscii(aaToken[2], ARRAYSIZE(aaToken[2])))
				{
					m_pMainlogic->m_pLog->Log("Slot[%d] Error, define name must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE, psParams->slotIndex);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, define name must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					retval = WriteKey(psSlotInfo, FILETYPE_DEFINES_MOSFET, pinNumber, aaToken[2], pResp, LenResp);
					if (retval != OK)
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] Failed to define <%d> <%s>", psParams->slotIndex, pinNumber, aaToken[2]);
					}
					else
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] Defined MOSFET %d as '%s'", psParams->slotIndex, pinNumber, aaToken[2]);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Defined MOSFET %d as '%s'", pinNumber, aaToken[2]);
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
			if (CCore::IsLetter(aaToken[1][0]))
				isName = true;

			if (isName)
			{
				for (i = 0; i <= 8; ++i)
				{
					retval = ReadKey(psSlotInfo->aUsername, FILETYPE_DEFINES_MOSFET, i, aRead, ARRAYSIZE(aRead), pResp, LenResp);
					if (retval != OK)
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] Failed to read key %d", psParams->slotIndex, i);
					}
					else // compare name
					{
						if (CCore::StringCompareNocase(aRead, aaToken[1], ARRAYSIZE(aRead)) == 0)
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

			// check MOSFET number
			if (!m_Hardware.IsMosfetValid(pinNumber))
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Error, name not found or MOSFET number outside " CSERVER_COM_MOSFET_RANGE, psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name not found or MOSFET number outside " CSERVER_COM_MOSFET_RANGE);
				strncat(pResp, aBufTemp, LenResp);
			}
			else
			{
				// check state
				isName = false;
				pinState = -1;

				// check if state is name or number (or empty)
				if (CCore::IsLetter(aaToken[2][0]) || strlen(aaToken[2]) == 0)
					isName = true;

				// check on/off/high/low
				if (isName)
				{
					if (CCore::StringCompareNocase(aaToken[2], "on", ARRAYSIZE(aaToken[2])) == 0)
						pinState = 1;
					else if (CCore::StringCompareNocase(aaToken[2], "off", ARRAYSIZE(aaToken[2])) == 0)
						pinState = 0;
					else if (CCore::StringCompareNocase(aaToken[2], "high", ARRAYSIZE(aaToken[2])) == 0)
						pinState = 1;
					else if (CCore::StringCompareNocase(aaToken[2], "low", ARRAYSIZE(aaToken[2])) == 0)
						pinState = 0;
				}
				else
				{
					pinState = atoi(aaToken[2]);
				}

				// check MOSFET state validity
				if (pinState < 0 || pinState > 1)
				{
					m_pMainlogic->m_pLog->Log("Slot[%d] Error unknown MOSFET state '%s'", psParams->slotIndex, aaToken[2]);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error unknown MOSFET state '%s'", aaToken[2]);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					// set MOSFET
					retval = m_Hardware.SetMosfet(pinNumber, pinState);

					if (retval != 0)
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] Failed to write MOSFET, returned %d", psParams->slotIndex, retval);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to write MOSFET, returned %d", retval);
						strncat(pResp, aBufTemp, LenResp);
					}
					else
					{
						m_pMainlogic->m_pLog->Log("Slot[%d] MOSFET %s (%d) --> %s", psParams->slotIndex, aaToken[1], pinNumber, aaToken[2]);
						snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "MOSFET %s (%d) --> %s", aaToken[1], pinNumber, aaToken[2]);
						strncat(pResp, aBufTemp, LenResp);
					}
				}
			}
			break;

		case COM_MOSREAD:
			m_pMainlogic->m_pLog->Log("Slot[%d] Implement function 'mosread' please!", psParams->slotIndex);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Implement function 'mosread' please!");
			strncat(pResp, aBufTemp, LenResp);
			break;

		case COM_MOSCLEAR:
			retval = m_Hardware.ClearMosfet();

			if (retval != 0)
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Failed to clear MOSFETs, returned %d", psParams->slotIndex, retval);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to clear MOSFETs, returned %d", retval);
				strncat(pResp, aBufTemp, LenResp);
			}
			else
			{
				m_pMainlogic->m_pLog->Log("Slot[%d] Cleared MOSFETs", psParams->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Cleared MOSFETs");
				strncat(pResp, aBufTemp, LenResp);
			}
			break;

		case COM_ECHO:
			randNr = rand() % ARRAYSIZE(gaaEchoes);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "You shouted... you hear '%s'", gaaEchoes[randNr]);
			strncat(pResp, aBufTemp, LenResp);
			m_pMainlogic->m_pLog->Log("Echoed '%s'", pResp);
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
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Unknown command '%s', use '" CSERVER_COMMAND_STRING_HELP "'", aaToken[0]);
		strncat(pResp, aBufTemp, LenResp);
	}
	else if (respondParametersWrong)
	{
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Invalid parameters, use '" CSERVER_COMMAND_STRING_HELP "'");
		strncat(pResp, aBufTemp, LenResp);
	}

	// add newline to buffer
	strncat(pResp, "\n", LenResp);

	// uppercase letters
	// CCore::StringToUpper(pResp, LenResp);
}

int CServer::IsCommandExecutable(S_SLOTINFO *psSlotInfo, int Flags)
{
	int isExecutable = false;

	// only logged in can execute flag: logged in
	if (psSlotInfo->loggedIn)
	{
		if ((Flags & CSERVER_COMFLAG_EXEC_LOGGEDIN))
			isExecutable = true;
	}
	else // only logged out can execute flag: logged out
	{
		if ((Flags & CSERVER_COMFLAG_EXEC_LOGGEDOUT))
			isExecutable = true;
	}

	// only activated can execute flag: activatedonly
	if (!psSlotInfo->activated)
	{
		if ((Flags & CSERVER_COMFLAG_EXEC_ACTIVATEDONLY))
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
		if ((Flags & CSERVER_COMFLAG_VISI_LOGGEDIN))
			isVisible = true;
	}
	else // only logged out can see flag: logged out
	{
		if ((Flags & CSERVER_COMFLAG_VISI_LOGGEDOUT))
			isVisible = true;
	}

	// only activated can see flag: activatedonly
	if (!psSlotInfo->activated)
	{
		if ((Flags & CSERVER_COMFLAG_VISI_ACTIVATEDONLY))
			isVisible = false;
	}

	return isVisible;
}

int CServer::ResetHardware()
{
	int retval = 0;

	// clear GPIOs
	m_Hardware.ClearGpio();

	// clear MOSFETs
	retval = m_Hardware.ClearMosfet();
	if (retval != 0)
		m_pMainlogic->m_pLog->Log("%s: Failed to clear MOSFETs", __FUNCTION__);

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
		switch (FileType)
		{
		case FILETYPE_ACCOUNT:
			snprintf(pFullpath, LenFullpath, "%s/_%s", CSERVER_FOLDERPATH_ACCOUNTS, pUsername);
			return;
			break;

		case FILETYPE_DEFINES_GPIO:
			snprintf(pFullpath, LenFullpath, "%s/_%s", CSERVER_FOLDERPATH_DEFINES_GPIO, pUsername);
			return;
			break;

		case FILETYPE_DEFINES_MOSFET:
			snprintf(pFullpath, LenFullpath, "%s/_%s", CSERVER_FOLDERPATH_DEFINES_MOSFET, pUsername);
			return;
			break;

		case FILETYPE_LOG:
			snprintf(pFullpath, LenFullpath, "%s", m_pMainlogic->m_pLog->GetLogFilename());
			return;
			break;
		}
	}
}

int CServer::AccountAction(E_ACCOUNTACTIONS AccountAction, S_SLOTINFO *psSlotInfo, const char *pUsername, const char *pPassword, int *pWrongCredentials, char *pError, size_t LenError)
{
	FILE *pFile = 0;
	char aFilename[CSERVER_MAX_LEN_USERFILES] = {0};
	char aFilepath[MAX_LEN_FILEPATH] = {0};
	char aBufTemp[CSERVER_NET_BUFFERSIZE] = {0};
	int len = 0;
	int fileCount = 0;
	int retval = 0;
	char aRead[CSERVER_MAX_LEN_LINES] = {0};
	int fileExists = false;

	// reset wrong credentials variable
	if (pWrongCredentials)
		*pWrongCredentials = false;

	// register / login check credentials for validity
	if (AccountAction == ACCACTION_REGISTER || AccountAction == ACCACTION_LOGIN)
	{
		// check username length
		len = strnlen(pUsername, CSERVER_MAX_LEN_USERFILES);
		if (len < CSERVER_MIN_LEN_CREDENTIALS || len > CSERVER_MAX_LEN_CREDENTIALS)
		{
			m_pMainlogic->m_pLog->Log("%s: Username too short/long", __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Username must be " STRINGIFY_VALUE(CSERVER_MIN_LEN_CREDENTIALS) " - " STRINGIFY_VALUE(CSERVER_MAX_LEN_CREDENTIALS) " characters");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check username ascii
		if (!CCore::CheckStringAscii(pUsername, CSERVER_MAX_LEN_CREDENTIALS + 1))
		{
			m_pMainlogic->m_pLog->Log("%s: Username must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE, __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Username must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE);
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check password length
		len = strnlen(pPassword, CSERVER_MAX_LEN_USERFILES);
		if (len < CSERVER_MIN_LEN_CREDENTIALS || len > CSERVER_MAX_LEN_CREDENTIALS)
		{
			m_pMainlogic->m_pLog->Log("%s: Password too short/long", __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Password must be " STRINGIFY_VALUE(CSERVER_MIN_LEN_CREDENTIALS) " - " STRINGIFY_VALUE(CSERVER_MAX_LEN_CREDENTIALS) " characters");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check password ascii
		if (!CCore::CheckStringAscii(pPassword, CSERVER_MAX_LEN_CREDENTIALS + 1))
		{
			m_pMainlogic->m_pLog->Log("%s: Password must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE, __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Password must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE);
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check if account file already existing
		MakeFilepath(FILETYPE_ACCOUNT, pUsername, true, aFilename, ARRAYSIZE(aFilename));
		fileExists = CCore::CheckFileExists(CSERVER_FOLDERPATH_ACCOUNTS, aFilename, ARRAYSIZE(aFilename));
	}

	switch (AccountAction)
	{
	case ACCACTION_REGISTER:
		// if account file already exists
		if (fileExists != false) // Treat ERROR as "file exists"
		{
			m_pMainlogic->m_pLog->Log("%s: File already exists", __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "User already exists");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check if any more registrations allowed
		fileCount = CCore::CountFilesDirectory(CSERVER_FOLDERPATH_ACCOUNTS);
		if (fileCount >= CSERVER_MAX_ACCOUNTS)
		{
			m_pMainlogic->m_pLog->Log("%s: Too many accounts exist %d/%d", __FUNCTION__, fileCount, CSERVER_MAX_ACCOUNTS);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Account limit reached, no more registration allowed");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// make filepath
		MakeFilepath(FILETYPE_ACCOUNT, pUsername, false, aFilepath, ARRAYSIZE(aFilepath));

		// create file
		pFile = fopen(aFilepath, "w");
		if (!pFile)
		{
			m_pMainlogic->m_pLog->Log("%s: Error opening file for account", __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "An error occurred, please try again later");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// write account data
		fprintf(pFile, "%s\n", pUsername);
		fprintf(pFile, "%s\n", pPassword);
		fprintf(pFile, "%d\n", 0); // activated

		// close file
		fclose(pFile);

		// create defines file GPIO
		retval = CreateDefinesFile(FILETYPE_DEFINES_GPIO, pUsername, pError, LenError);
		if (retval != OK)
			return ERROR;

		// create defines file MOSFET
		retval = CreateDefinesFile(FILETYPE_DEFINES_MOSFET, pUsername, pError, LenError);
		if (retval != OK)
			return ERROR;
		break;

	case ACCACTION_LOGIN:
		// if account file already exists
		if (fileExists != true)
		{
			m_pMainlogic->m_pLog->Log("%s: Username '%s' does not exist", __FUNCTION__, pUsername);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), CSERVER_RESPONSE_LOGIN_FAILED);
			strncat(pError, aBufTemp, LenError);

			if (pWrongCredentials)
				*pWrongCredentials = true;

			return ERROR;
		}

		// compare passwords
		retval = ReadKey(pUsername, FILETYPE_ACCOUNT, ACCKEY_PASSWORD, aRead, ARRAYSIZE(aRead), pError, LenError);
		if (retval != OK)
			return ERROR;

		if (strcmp(aRead, pPassword) != 0)
		{
			m_pMainlogic->m_pLog->Log("%s: Password '%s' incorrect", __FUNCTION__, pPassword);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), CSERVER_RESPONSE_LOGIN_FAILED);
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
		retval = ReadKey(pUsername, FILETYPE_ACCOUNT, ACCKEY_ACTIVATED, aRead, ARRAYSIZE(aRead), pError, LenError);
		if (retval != OK)
			return ERROR;

		psSlotInfo->activated = atoi(aRead);

		// reset failed logins
		psSlotInfo->failedLogins = 0;

		// remove IP from suspicious list if the account is activated, else non activated accounts could exploit the counter reset by logging in with a deactivated account
		if (psSlotInfo->activated)
			RemoveSuspiciousIP(psSlotInfo->clientIP);
		break;

	case ACCACTION_LOGOUT:
		ResetSlot(psSlotInfo);
		break;
	}

	return OK;
}

int CServer::CreateDefinesFile(E_FILETYPES FileType, const char *pUsername, char *pError, size_t LenError)
{
	FILE *pFile = 0;
	char aFilepath[MAX_LEN_FILEPATH] = {0};
	char aBufTemp[CSERVER_NET_BUFFERSIZE] = {0};
	int i = 0;

	// check file type
	if (FileType != FILETYPE_DEFINES_GPIO && FileType != FILETYPE_DEFINES_MOSFET)
	{
		m_pMainlogic->m_pLog->Log("%s: Error, invalid filetype %d", __FUNCTION__, FileType);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "An error occurred");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	// make filepath
	MakeFilepath(FileType, pUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// create file
	pFile = fopen(aFilepath, "w");
	if (!pFile)
	{
		m_pMainlogic->m_pLog->Log("%s: Error opening file for defines", __FUNCTION__);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "An error occurred");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	// write stuff
	for (i = 0; i < CSERVER_MAX_LINES; ++i)
		fprintf(pFile, "\n");

	fclose(pFile);

	return OK;
}

int CServer::WriteKey(S_SLOTINFO *psSlotInfo, E_FILETYPES FileType, int Key, const char *pValue, char *pError, size_t LenError)
{
	FILE *pFile = 0;
	char aFilepath[MAX_LEN_FILEPATH] = {0};
	int line = 0;
	char aLine[CSERVER_MAX_LINES][CSERVER_MAX_LEN_LINES] = {{0}};
	signed char ch = 0;
	int chCount = 0;
	int lineIndex = 0;
	int bufIndex = 0;
	int i = 0;
	int eofReached = false;
	char aBufTemp[CSERVER_NET_BUFFERSIZE] = {0};

	// filepath
	MakeFilepath(FileType, psSlotInfo->aUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// read
	pFile = fopen(aFilepath, "r");
	if (!pFile)
	{
		m_pMainlogic->m_pLog->Log("%s: Error opening file for read", __FUNCTION__);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error opening file for read");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	// read all lines
	while (1)
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
		m_pMainlogic->m_pLog->Log("%s: Error opening file for write", __FUNCTION__);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error opening file for write");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	for (i = 0; i < ARRAYSIZE(aLine); ++i)
		fprintf(pFile, aLine[i]);

	fclose(pFile);

	return OK;
}

int CServer::ReadKey(const char *pUsername, E_FILETYPES FileType, int Key, char *pKey, size_t LenKey, char *pError, size_t LenError)
{
	FILE *pFile = 0;
	char aFilepath[MAX_LEN_FILEPATH] = {0};
	int line = 0;
	char aLine[CSERVER_MAX_LINES][CSERVER_MAX_LEN_LINES] = {0};
	signed char ch = 0;
	int chCount = 0;
	int lineIndex = 0;
	int bufIndex = 0;
	int i = 0;
	int eofReached = false;
	char aBufTemp[CSERVER_NET_BUFFERSIZE] = {0};

	// filepath
	MakeFilepath(FileType, pUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// read
	pFile = fopen(aFilepath, "r");
	if (!pFile)
	{
		m_pMainlogic->m_pLog->Log("%s: Error opening file for read", __FUNCTION__);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error opening file for read");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	// read all lines
	while (1)
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
				CCore::StringCopyIgnore(pKey, aLine[lineIndex], LenKey, "\n");
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

	m_pMainlogic->m_pLog->Log("%s: Error, line %d not read", __FUNCTION__, Key);
	snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error reading line %d", Key);
	strncat(pError, aBufTemp, LenError);

	return ERROR;
}

int CServer::RemoveFile(S_SLOTINFO *psSlotInfo, E_FILETYPES FileType, char *pError, size_t LenError)
{
	int retval = 0;
	char aFilepath[MAX_LEN_FILEPATH] = {0};
	char aBufTemp[CSERVER_NET_BUFFERSIZE] = {0};

	// filepath
	MakeFilepath(FileType, psSlotInfo->aUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// remove
	retval = CCore::RemoveFile(aFilepath);
	if (retval != OK)
	{
		m_pMainlogic->m_pLog->Log("%s: Error removing file '%s'", __FUNCTION__, aFilepath);
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
	int amtSlots = ARRAYSIZE(m_sServerInfo.aBannedIPs);
	int freeIndex = -1;

	// check if a ban-slot is free
	for (i = 0; i < amtSlots; ++i)
	{
		if (m_sServerInfo.aBannedIPs[i] != 0)
			banSlotCount++;
		else if (freeIndex == -1)
			freeIndex = i;
	}

	if (banSlotCount >= amtSlots)
	{
		m_pMainlogic->m_pLog->Log("%s: All banned IP slots occupied (%d)", __FUNCTION__, amtSlots);
		return ERROR;
	}

	// ban IP
	m_sServerInfo.aBannedIPs[freeIndex] = IP;
	m_sServerInfo.aBanStartTime[freeIndex] = time(0);
	m_sServerInfo.aBanDuration[freeIndex] = Duration;

	return OK;
}

int CServer::CheckIPBanned(S_SLOTINFO *psSlotInfo, struct tm *psTime)
{
	int i = 0;
	time_t currentTime = time(0);
	time_t timeRemaining = 0;

	for (i = 0; i < ARRAYSIZE(m_sServerInfo.aBannedIPs); ++i)
	{
		if (psSlotInfo->clientIP == m_sServerInfo.aBannedIPs[i])
		{
			// calculate remaining ban time
			timeRemaining = (m_sServerInfo.aBanStartTime[i] + m_sServerInfo.aBanDuration[i]) - currentTime;
			*psTime = *localtime(&timeRemaining);
			return true;
		}
	}

	return false;
}

void CServer::CleanOldSession(S_SLOTINFO *psSlotInfo)
{
	// reset slot and close socket to free fd
	CloseClientSocket(psSlotInfo);

	// reset slot
	memset(psSlotInfo, 0, sizeof(S_SLOTINFO));

	// cancel unused thread
	CCore::DetachThreadSafely(&psSlotInfo->thrClientConnection); // do not check return value, the thread could have been cancelled by itself from disconnecting client or banning
}

void CServer::CloseClientSocket(S_SLOTINFO *psSlotInfo)
{
	ResetSlot(psSlotInfo);
	close(psSlotInfo->connfd);
}

void CServer::ResetSlot(S_SLOTINFO *psSlotInfo)
{
	memset(psSlotInfo->aUsername, 0, CSERVER_MAX_LEN_CREDENTIALS + 1);
	psSlotInfo->loggedIn = false;
	psSlotInfo->activated = false;
	psSlotInfo->failedLogins = 0;
	psSlotInfo->banned = false;
}

int CServer::AddSuspiciousIP(in_addr_t IP)
{
	int knownSlotIndex = -1;
	int i = 0;
	int amtSlots = ARRAYSIZE(m_sServerInfo.aSuspiciousIPs);
	int addedIP = false;

	// check if the IP is already known
	for (i = 0; i < amtSlots; ++i)
	{
		if (m_sServerInfo.aSuspiciousIPs[i] == IP)
		{
			knownSlotIndex = i;
			break;
		}
	}

	// if slot was found, increase the sus counter
	if (knownSlotIndex != -1)
	{
		m_sServerInfo.aSuspiciousAttempts[knownSlotIndex]++;
		addedIP = true;

		// refresh suspicious start time
		m_sServerInfo.aSuspiciousStartTime[knownSlotIndex] = time(0);
	}
	else // add the IP to the first free slot
	{
		for (i = 0; i < amtSlots; ++i)
		{
			if (m_sServerInfo.aSuspiciousIPs[i] == 0)
			{
				m_sServerInfo.aSuspiciousIPs[i] = IP;
				m_sServerInfo.aSuspiciousAttempts[i]++;
				m_sServerInfo.aSuspiciousStartTime[i] = time(0);
				addedIP = true;
				break;
			}
		}
	}

	// if no IP matched and all slots full, error
	if (!addedIP)
	{
		m_pMainlogic->m_pLog->Log("%s: All suspicious IP slots occupied (%d)", __FUNCTION__, amtSlots);
		return ERROR;
	}

	return OK;
}

void CServer::RemoveSuspiciousIP(in_addr_t IP)
{
	int knownSlotIndex = -1;
	int i = 0;
	int amtSlots = ARRAYSIZE(m_sServerInfo.aSuspiciousIPs);
	int addedIP = false;

	// check if the IP is already known
	for (i = 0; i < amtSlots; ++i)
	{
		if (m_sServerInfo.aSuspiciousIPs[i] == IP)
		{
			m_sServerInfo.aSuspiciousIPs[i] = 0;
			m_sServerInfo.aSuspiciousAttempts[i] = 0;
			m_sServerInfo.aSuspiciousStartTime[i] = 0;

			return;
		}
	}
}

int CServer::GetSuspiciousAttempts(in_addr_t IP)
{
	int i = 0;
	int attempts = 0;

	// check if IP in list
	for (i = 0; i < ARRAYSIZE(m_sServerInfo.aSuspiciousIPs); ++i)
	{
		if (IP == m_sServerInfo.aSuspiciousIPs[i])
		{
			attempts = m_sServerInfo.aSuspiciousAttempts[i];
			break;
		}
	}

	return attempts;
}

struct in_addr CServer::GetIPStruct(in_addr_t IP)
{
	struct in_addr sInAddr = {0};

	sInAddr.s_addr = IP;

	return sInAddr;
}