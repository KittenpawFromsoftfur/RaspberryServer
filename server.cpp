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

CServer::CServer(CMainlogic *pMainlogic, int ServerPort, int UppercaseResponse)
{
	m_pMainlogic = pMainlogic;

	if (ServerPort >= 0)
		m_ServerPort = ServerPort;
	else
		m_ServerPort = SERVER_PORT_DEFAULT;

	m_UppercaseResponse = UppercaseResponse;
	memset(&m_asSlotInfo, 0, sizeof(m_asSlotInfo));
}

CServer::~CServer()
{
	CCore::DetachThreadSafely(&m_ThrUpdate);

	for (int i = 0; i < ARRAYSIZE(m_asSlotInfo); ++i)
		CCore::DetachThreadSafely(&m_asSlotInfo[i].thrClientConnection);
}

int CServer::Run()
{
	int retval = 0;
	int fdConnectionCurrent = 0;
	struct sigaction sSigAction = {0};
	struct sockaddr_in sSockaddrCurrent = {0};
	socklen_t socklenCurrent = 0;
	struct sockaddr_in sServAddr = {0};
	int slotIndex = 0;
	int sockOptValue = 0;

	// make process ignore SIGPIPE signal, so it will not exit when writing to disconnected socket
	sSigAction.sa_handler = SIG_IGN;
	retval = sigaction(SIGPIPE, &sSigAction, NULL);
	if (retval != 0)
	{
		m_pMainlogic->m_Log.Log("%s: Failed to ignore the SIGPIPE signal", __FUNCTION__);
		m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
		return ERROR;
	}

	// reset hardware
	retval = ResetHardware();
	if (retval != OK)
	{
		m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
		return ERROR;
	}

	// randomize for echo
	srand(time(0));

	// create accounts folder paths
	retval = CCore::Mkdir(CSERVER_FOLDERPATH_ACCOUNTS);
	if (retval != OK)
	{
		m_pMainlogic->m_Log.Log("%s: Failed to make folder \"%s\"", CSERVER_FOLDERPATH_ACCOUNTS);
		m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
		return ERROR;
	}

	// create defines GPIO folder path
	retval = CCore::Mkdir(CSERVER_FOLDERPATH_DEFINES_GPIO);
	if (retval != OK)
	{
		m_pMainlogic->m_Log.Log("%s: Failed to make folder \"%s\"", CSERVER_FOLDERPATH_DEFINES_GPIO);
		m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
		return ERROR;
	}

	// create defines MOSFET folder path
	retval = CCore::Mkdir(CSERVER_FOLDERPATH_DEFINES_MOSFET);
	if (retval != OK)
	{
		m_pMainlogic->m_Log.Log("%s: Failed to make folder \"%s\"", CSERVER_FOLDERPATH_DEFINES_MOSFET);
		m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
		return ERROR;
	}

	// spawn update thread
	m_ThrUpdate = std::thread(&CServer::Update, this);

	// create server socket
	m_FDListen = socket(AF_INET, SOCK_STREAM, 0);
	if (m_FDListen == -1)
	{
		m_pMainlogic->m_Log.Log("%s: Failed to create server socket, errno: %d", __FUNCTION__, errno);
		m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
		return ERROR;
	}

	// server socket option: allow reusing socket
	sockOptValue = 1;
	retval = setsockopt(m_FDListen, SOL_SOCKET, SO_REUSEADDR, &sockOptValue, sizeof(sockOptValue));
	if (retval != 0)
	{
		m_pMainlogic->m_Log.Log("%s: Failed to set socket option: SO_REUSEADDR, errno: %d", __FUNCTION__, errno);
		m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
		return ERROR;
	}

	// set up server settings
	sServAddr.sin_family = AF_INET;
	sServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	sServAddr.sin_port = htons(m_ServerPort);

	// bind server socket with settings
	retval = bind(m_FDListen, (struct sockaddr *)&sServAddr, sizeof(sServAddr));
	if (retval != 0)
	{
		m_pMainlogic->m_Log.Log("%s: Failed to bind, errno: %d", __FUNCTION__, errno);
		m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
		return ERROR;
	}

	// start listening
	retval = listen(m_FDListen, CSERVER_MAX_LEN_LISTENQUEUE);
	if (retval != 0)
	{
		m_pMainlogic->m_Log.Log("%s: Failed to listen, errno: %d", __FUNCTION__, errno);
		m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
		return ERROR;
	}

	// server loop
	m_pMainlogic->m_Log.Log("Server running port %d, uppercase response %s...", m_ServerPort, m_UppercaseResponse ? "enabled" : "disabled");

	while (1)
	{
		// accept incoming connections
		m_pMainlogic->m_Log.Log("Ready for new connection...");

		socklenCurrent = sizeof(struct sockaddr);
		fdConnectionCurrent = accept(m_FDListen, (struct sockaddr *)&sSockaddrCurrent, &socklenCurrent);

		// check connection details
		m_pMainlogic->m_Log.Log("Accepted port %d, IP %s", htons(sSockaddrCurrent.sin_port), inet_ntoa(sSockaddrCurrent.sin_addr));

		// check if a client with the same IP has been connected before
		slotIndex = -1;

		for (int i = 0; i < ARRAYSIZE(m_asSlotInfo); ++i)
		{
			if (sSockaddrCurrent.sin_addr.s_addr == m_asSlotInfo[i].clientIP)
			{
				slotIndex = i;
				break;
			}
		}

		// same IP, clean up old thread
		if (slotIndex > -1)
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Same IP detected, cleaning up...", slotIndex);
			CleanOldSession(&m_asSlotInfo[slotIndex]);
		}

		// find first free client index
		slotIndex = -1;

		for (int i = 0; i < ARRAYSIZE(m_asSlotInfo); ++i)
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
			m_pMainlogic->m_Log.Log("Max client connections reached, no thread created");
			close(fdConnectionCurrent);
			continue;
		}

		// client socket option: enable keepalive
		sockOptValue = 1;
		retval = setsockopt(fdConnectionCurrent, SOL_SOCKET, SO_KEEPALIVE, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			m_pMainlogic->m_Log.Log("%s: Failed to set socket option: SO_KEEPALIVE, errno: %d", __FUNCTION__, errno);
			close(fdConnectionCurrent);
			m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
			return ERROR;
		}

		// client socket option: keepalive idle time
		sockOptValue = 10; // s
		retval = setsockopt(fdConnectionCurrent, IPPROTO_TCP, TCP_KEEPIDLE, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			m_pMainlogic->m_Log.Log("%s: Failed to set socket option: TCP_KEEPIDLE, errno: %d", __FUNCTION__, errno);
			close(fdConnectionCurrent);
			m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
			return ERROR;
		}

		// client socket option: keepalive interval
		sockOptValue = 10; // s
		retval = setsockopt(fdConnectionCurrent, IPPROTO_TCP, TCP_KEEPINTVL, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			m_pMainlogic->m_Log.Log("%s: Failed to set socket option: TCP_KEEPINTVL, errno: %d", __FUNCTION__, errno);
			close(fdConnectionCurrent);
			m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
			return ERROR;
		}

		// client socket option: keepalive probes
		sockOptValue = 2;
		retval = setsockopt(fdConnectionCurrent, IPPROTO_TCP, TCP_KEEPCNT, &sockOptValue, sizeof(sockOptValue));
		if (retval != 0)
		{
			m_pMainlogic->m_Log.Log("%s: Failed to set socket option: TCP_KEEPCNT, errno: %d", __FUNCTION__, errno);
			close(fdConnectionCurrent);
			m_pMainlogic->SetThreadStatus(CMainlogic::THR_SERVERRUN, CMainlogic::THS_FAILED);
			return ERROR;
		}

		// spawn client connection
		SpawnClientConnection(sSockaddrCurrent.sin_addr.s_addr, fdConnectionCurrent, slotIndex);
	}

	return OK;
}

int CServer::OnExitApplication()
{
	int retval = 0;
	int hasError = false;

	retval = ResetHardware();
	if (retval != OK)
		hasError = true;

	// detach update thread
	retval = CCore::DetachThreadSafely(&m_ThrUpdate);
	if (retval != OK)
		m_pMainlogic->m_Log.Log("Failed to detach update thread");

	// cancel client connection threads and close sockets
	for (int i = 0; i < ARRAYSIZE(m_asSlotInfo); ++i)
	{
		retval = CCore::DetachThreadSafely(&m_asSlotInfo[i].thrClientConnection);
		if (retval != OK)
			m_pMainlogic->m_Log.Log("Failed to detach client connection thread");

		CloseClientSocket(&m_asSlotInfo[i]);
	}

	// close server socket
	close(m_FDListen);

	if (hasError)
		return ERROR;

	return OK;
}

void CServer::SpawnClientConnection(unsigned long ClientIP, int FDConnection, int SlotIndex)
{
	S_SLOTINFO *psSlotInfo = &m_asSlotInfo[SlotIndex];

	// assign slot infos
	psSlotInfo->clientIP = ClientIP;
	psSlotInfo->fdSocket = FDConnection;
	psSlotInfo->slotIndex = SlotIndex;

	// create thread
	m_pMainlogic->m_Log.Log("Slot[%d] Creating new thread...", SlotIndex);
	psSlotInfo->thrClientConnection = std::thread(&CServer::ClientConnection, this, psSlotInfo);
}

int CServer::ClientConnection(S_SLOTINFO *psSlotInfo)
{
	int retval = 0;
	char aBufRaw[CSERVER_NET_BUFFERSIZE] = {0};
	char aBuf[CSERVER_NET_BUFFERSIZE] = {0};
	char aBufResp[CSERVER_NET_BUFFERSIZE] = {0};
	struct tm sTime = {0};
	struct timeval sTimeval = {0};
	fd_set sFDSet = {0};
	int timeoutOccurred = false;

	// check if IP banned, then close socket
	if (CheckIPBanned(psSlotInfo, &sTime))
	{
		m_pMainlogic->m_Log.Log("Slot[%d] IP '%s' still banned for %dh %dm %ds, cleaning up...", psSlotInfo->slotIndex, inet_ntoa(GetIPStruct(psSlotInfo->clientIP)), sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
		snprintf(aBufResp, ARRAYSIZE(aBufResp), CSERVER_RESPONSE_PREFIX "You are banned for %dh %dm %ds\n", sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
		write(psSlotInfo->fdSocket, aBufResp, ARRAYSIZE(aBufResp));
		CleanOldSession(psSlotInfo);
		return ERROR;
	}

	// read from client loop
	while (1)
	{
		// write response prefix to client
		if (!timeoutOccurred)
			write(psSlotInfo->fdSocket, CSERVER_RESPONSE_PREFIX, ARRAYSIZE(CSERVER_RESPONSE_PREFIX));

		// add server file descriptor to select watch list
		FD_ZERO(&sFDSet);
		FD_SET(psSlotInfo->fdSocket, &sFDSet);

		// set select timeout
		memset(&sTimeval, 0, sizeof(sTimeval));
		sTimeval.tv_sec = CSERVER_TIMEOUT_CLIENTCONNECTION_READ;

		// read from connection until bytes are read
		retval = select(psSlotInfo->fdSocket + 1, &sFDSet, NULL, NULL, &sTimeval);

		// data available
		if (retval > 0)
		{
			timeoutOccurred = false;

			memset(aBufRaw, 0, ARRAYSIZE(aBufRaw));
			retval = read(psSlotInfo->fdSocket, aBufRaw, ARRAYSIZE(aBufRaw));

			// copy aBuf and ignore certain characters
			memset(aBuf, 0, ARRAYSIZE(aBuf));
			CCore::StringCopyIgnore(aBuf, aBufRaw, ARRAYSIZE(aBuf), "\r\n");

			// check if client disconnected and close socket
			if (strnlen(aBufRaw, ARRAYSIZE(aBuf)) == 0)
			{
				m_pMainlogic->m_Log.Log("Slot[%d] Disconnected", psSlotInfo->slotIndex);

				CleanOldSession(psSlotInfo);
				return ERROR;
			}
			else if (strnlen(aBuf, ARRAYSIZE(aBuf)) > 0) // client sent message
			{
				m_pMainlogic->m_Log.Log("Slot[%d] <%s>", psSlotInfo->slotIndex, aBuf);

				memset(aBufResp, 0, ARRAYSIZE(aBufResp));
				retval = ParseMessage(psSlotInfo, aBuf, aBufResp, ARRAYSIZE(aBufResp));
				if (retval != OK)
				{
					m_pMainlogic->m_Log.Log("%s: Slot[%d] Error parsing message", __FUNCTION__, psSlotInfo->slotIndex);
				}

				// send response
				if (strlen(aBufResp) > 0)
					write(psSlotInfo->fdSocket, aBufResp, ARRAYSIZE(aBufResp));

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
			m_pMainlogic->m_Log.Log("Slot[%d] Error for select(), errno: %d\n", psSlotInfo->slotIndex, errno);
		}
	}

	return OK;
}

int CServer::Update()
{
	time_t currentTime = 0;

	while (1)
	{
		// get current time
		currentTime = time(0);

		// update IP bans
		for (int i = 0; i < ARRAYSIZE(m_aBannedIPs); ++i)
		{
			if (m_aBannedIPs[i] == 0)
				continue;

			if (currentTime >= m_aBanStartTime[i] + m_aBanDuration[i])
			{
				// remove IP from suspicious list
				RemoveSuspiciousIP(m_aBannedIPs[i]);

				// unban IP
				m_pMainlogic->m_Log.Log("IP[%d] '%s' has been unbanned", i, inet_ntoa(GetIPStruct(m_aBannedIPs[i])));
				m_aBannedIPs[i] = 0;
				m_aBanStartTime[i] = 0;
				m_aBanDuration[i] = 0;
			}
		}

		// update suspicious IPs list
		for (int i = 0; i < ARRAYSIZE(m_aSuspiciousIPs); ++i)
		{
			if (m_aSuspiciousIPs[i] == 0)
				continue;

			if (currentTime >= m_aSuspiciousStartTime[i] + CSERVER_SUSPICIOUS_FALLOFF_TIME)
			{
				m_pMainlogic->m_Log.Log("Unlisted suspicious IP[%d] '%s'", i, inet_ntoa(GetIPStruct(m_aSuspiciousIPs[i])));
				RemoveSuspiciousIP(m_aSuspiciousIPs[i]);
			}
		}

		usleep(CSERVER_DELAY_UPDATELOOP);
	}
}

int CServer::ParseMessage(S_SLOTINFO *psSlotInfo, const char *pMsg, char *pResp, size_t LenResp)
{
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
	EvaluateTokens(psSlotInfo, aaToken, pResp, LenResp, pMsg);

	return OK;
}

void CServer::EvaluateTokens(S_SLOTINFO *psSlotInfo, char aaToken[CSERVER_MAX_TOKENS][CSERVER_MAX_LEN_TOKEN], char *pResp, size_t LenResp, const char *pMsgFull)
{
	int retval = 0;
	int foundMatch = false;
	int commandIndex = 0;
	char aBufTemp[CSERVER_NET_BUFFERSIZE] = {0};
	int randNr = 0;
	int commandExecutable = false;
	int commandVisible = false;
	int respondCommandUnknown = false;
	int respondParametersWrong = false;
	char aRead[CSERVER_MAX_LEN_LINES] = {0};
	int pinNumber = 0;
	int pinState = 0;
	int isName = 0;

	// look for command
	for (int i = 0; i < ARRAYSIZE(m_asCommands); ++i)
	{
		if (CCore::StringCompareNocase(aaToken[0], m_asCommands[i].aName, ARRAYSIZE(aaToken[0])) == 0)
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
		commandExecutable = IsCommandExecutable(psSlotInfo, m_asCommands[commandIndex].flags);

		// check if command is visible
		commandVisible = IsCommandVisible(psSlotInfo, m_asCommands[commandIndex].flags);
	}

	if (commandExecutable)
	{
		m_pMainlogic->m_Log.Log("Slot[%d] --> '%s'", psSlotInfo->slotIndex, m_asCommands[commandIndex].aName);

		switch (m_asCommands[commandIndex].ID)
		{
		case COM_HELP:
			ComHelp(psSlotInfo, pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp));
			break;

		case COM_ACTIVATEACCOUNT:
			retval = ComActivateAccount(psSlotInfo, pResp, LenResp, aaToken[1], aaToken[2], aaToken[3], ARRAYSIZE(aaToken[0]));
			if (retval != OK)
				respondCommandUnknown = true;
			break;

		case COM_REGISTER:
			ComRegister(psSlotInfo, pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp), aaToken[1], aaToken[2]);
			break;

		case COM_LOGIN:
			ComLogin(psSlotInfo, pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp), aaToken[1], aaToken[2]);
			break;

		case COM_LOGOUT:
			ComLogout(psSlotInfo, pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp));
			break;

		case COM_SHUTDOWN:
			ComShutdown(psSlotInfo);
			break;

		case COM_RUN:
			ComRun(psSlotInfo, pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp), pMsgFull);
			break;

		case COM_DELETE:
			retval = ComDelete(psSlotInfo, pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp), aaToken[1], ARRAYSIZE(aaToken[0]));
			if (retval != OK)
				respondParametersWrong = true;
			break;

		case COM_IO_DEFINE:
			ComIODefine(psSlotInfo, pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp), aaToken[1], aaToken[2], ARRAYSIZE(aaToken[0]));
			break;

		case COM_IO_SET:
			ComIOSet(psSlotInfo, pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp), aaToken[1], aaToken[2], ARRAYSIZE(aaToken[0]));
			break;

		case COM_IO_READ:
			ComIORead(psSlotInfo, pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp));
			break;

		case COM_IO_CLEAR:
			ComIOClear(psSlotInfo, pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp));
			break;

		case COM_ECHO:
			ComEcho(pResp, LenResp, aBufTemp, ARRAYSIZE(aBufTemp));
			break;

		case COM_EXIT:
			ComExit(psSlotInfo);
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
	if (m_UppercaseResponse)
		CCore::StringToUpper(pResp, LenResp);
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
	retval = m_Hardware.ClearIO(CHardware::GPIO);
	if (retval != OK)
		m_pMainlogic->m_Log.Log("%s: Failed to clear GPIOs", __FUNCTION__);

	// clear MOSFETs
	retval = m_Hardware.ClearIO(CHardware::MOSFET);
	if (retval != OK)
		m_pMainlogic->m_Log.Log("%s: Failed to clear MOSFETs", __FUNCTION__);

	return retval;
}

void CServer::GetFilepath(E_FILETYPES FileType, const char *pUsername, int FilenameOnly, char *pFullpath, size_t LenFullpath)
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
			snprintf(pFullpath, LenFullpath, "%s", m_pMainlogic->m_Log.GetLogFilename());
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
			m_pMainlogic->m_Log.Log("%s: Username too short/long", __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Username must be " STRINGIFY_VALUE(CSERVER_MIN_LEN_CREDENTIALS) " - " STRINGIFY_VALUE(CSERVER_MAX_LEN_CREDENTIALS) " characters");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check username ascii
		if (!CCore::CheckStringAscii(pUsername, CSERVER_MAX_LEN_CREDENTIALS + 1))
		{
			m_pMainlogic->m_Log.Log("%s: Username must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE, __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Username must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE);
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check password length
		len = strnlen(pPassword, CSERVER_MAX_LEN_USERFILES);
		if (len < CSERVER_MIN_LEN_CREDENTIALS || len > CSERVER_MAX_LEN_CREDENTIALS)
		{
			m_pMainlogic->m_Log.Log("%s: Password too short/long", __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Password must be " STRINGIFY_VALUE(CSERVER_MIN_LEN_CREDENTIALS) " - " STRINGIFY_VALUE(CSERVER_MAX_LEN_CREDENTIALS) " characters");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check password ascii
		if (!CCore::CheckStringAscii(pPassword, CSERVER_MAX_LEN_CREDENTIALS + 1))
		{
			m_pMainlogic->m_Log.Log("%s: Password must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE, __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Password must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE);
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check if account file already existing
		GetFilepath(FILETYPE_ACCOUNT, pUsername, true, aFilename, ARRAYSIZE(aFilename));
		fileExists = CCore::CheckFileExists(CSERVER_FOLDERPATH_ACCOUNTS, aFilename, ARRAYSIZE(aFilename));
	}

	switch (AccountAction)
	{
	case ACCACTION_REGISTER:
		// if account file already exists
		if (fileExists != false) // Treat ERROR as "file exists"
		{
			m_pMainlogic->m_Log.Log("%s: File already exists", __FUNCTION__);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "User already exists");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// check if any more registrations allowed
		fileCount = CCore::CountFilesDirectory(CSERVER_FOLDERPATH_ACCOUNTS);
		if (fileCount >= CSERVER_MAX_ACCOUNTS)
		{
			m_pMainlogic->m_Log.Log("%s: Too many accounts exist %d/%d", __FUNCTION__, fileCount, CSERVER_MAX_ACCOUNTS);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Account limit reached, no more registration allowed");
			strncat(pError, aBufTemp, LenError);
			return ERROR;
		}

		// make filepath
		GetFilepath(FILETYPE_ACCOUNT, pUsername, false, aFilepath, ARRAYSIZE(aFilepath));

		// create file
		pFile = fopen(aFilepath, "w");
		if (!pFile)
		{
			m_pMainlogic->m_Log.Log("%s: Error opening file for account", __FUNCTION__);
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
			m_pMainlogic->m_Log.Log("%s: Username '%s' does not exist", __FUNCTION__, pUsername);
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
			m_pMainlogic->m_Log.Log("%s: Password '%s' incorrect", __FUNCTION__, pPassword);
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

	// check file type
	if (FileType != FILETYPE_DEFINES_GPIO && FileType != FILETYPE_DEFINES_MOSFET)
	{
		m_pMainlogic->m_Log.Log("%s: Error, invalid filetype %d", __FUNCTION__, FileType);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "An error occurred");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	// make filepath
	GetFilepath(FileType, pUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// create file
	pFile = fopen(aFilepath, "w");
	if (!pFile)
	{
		m_pMainlogic->m_Log.Log("%s: Error opening file for defines", __FUNCTION__);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "An error occurred");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	// write stuff
	for (int i = 0; i < CSERVER_MAX_LINES; ++i)
		fprintf(pFile, "\n");

	fclose(pFile);

	return OK;
}

int CServer::WriteKey(S_SLOTINFO *psSlotInfo, E_FILETYPES FileType, E_ACCOUNTKEYS Key, const char *pValue, char *pError, size_t LenError)
{
	FILE *pFile = 0;
	char aFilepath[MAX_LEN_FILEPATH] = {0};
	int line = 0;
	char aLine[CSERVER_MAX_LINES][CSERVER_MAX_LEN_LINES] = {{0}};
	signed char ch = 0;
	int chCount = 0;
	int lineIndex = 0;
	int bufIndex = 0;
	int eofReached = false;
	char aBufTemp[CSERVER_NET_BUFFERSIZE] = {0};

	// filepath
	GetFilepath(FileType, psSlotInfo->aUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// read
	pFile = fopen(aFilepath, "r");
	if (!pFile)
	{
		m_pMainlogic->m_Log.Log("%s: Error opening file for read", __FUNCTION__);
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
		m_pMainlogic->m_Log.Log("%s: Error opening file for write", __FUNCTION__);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error opening file for write");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	for (int i = 0; i < ARRAYSIZE(aLine); ++i)
		fprintf(pFile, aLine[i]);

	fclose(pFile);

	return OK;
}

int CServer::ReadKey(const char *pUsername, E_FILETYPES FileType, E_ACCOUNTKEYS Key, char *pKey, size_t LenKey, char *pError, size_t LenError)
{
	FILE *pFile = 0;
	char aFilepath[MAX_LEN_FILEPATH] = {0};
	int line = 0;
	char aLine[CSERVER_MAX_LINES][CSERVER_MAX_LEN_LINES] = {0};
	signed char ch = 0;
	int chCount = 0;
	int lineIndex = 0;
	int bufIndex = 0;
	int eofReached = false;
	char aBufTemp[CSERVER_NET_BUFFERSIZE] = {0};

	// filepath
	GetFilepath(FileType, pUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// read
	pFile = fopen(aFilepath, "r");
	if (!pFile)
	{
		m_pMainlogic->m_Log.Log("%s: Error opening file for read", __FUNCTION__);
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

	m_pMainlogic->m_Log.Log("%s: Error, line %d not read", __FUNCTION__, Key);
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
	GetFilepath(FileType, psSlotInfo->aUsername, false, aFilepath, ARRAYSIZE(aFilepath));

	// remove
	retval = CCore::RemoveFile(aFilepath);
	if (retval != OK)
	{
		m_pMainlogic->m_Log.Log("%s: Error removing file '%s'", __FUNCTION__, aFilepath);
		snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error removing file");
		strncat(pError, aBufTemp, LenError);
		return ERROR;
	}

	return OK;
}

int CServer::BanIP(in_addr_t IP, time_t Duration)
{
	int banSlotCount = 0;
	int amtSlots = ARRAYSIZE(m_aBannedIPs);
	int freeIndex = -1;

	// check if a ban-slot is free
	for (int i = 0; i < amtSlots; ++i)
	{
		if (m_aBannedIPs[i] != 0)
			banSlotCount++;
		else if (freeIndex == -1)
			freeIndex = i;
	}

	if (banSlotCount >= amtSlots)
	{
		m_pMainlogic->m_Log.Log("%s: All banned IP slots occupied (%d)", __FUNCTION__, amtSlots);
		return ERROR;
	}

	// ban IP
	m_aBannedIPs[freeIndex] = IP;
	m_aBanStartTime[freeIndex] = time(0);
	m_aBanDuration[freeIndex] = Duration;

	return OK;
}

int CServer::CheckIPBanned(S_SLOTINFO *psSlotInfo, struct tm *psTime)
{
	time_t currentTime = time(0);
	time_t timeRemaining = 0;

	for (int i = 0; i < ARRAYSIZE(m_aBannedIPs); ++i)
	{
		if (psSlotInfo->clientIP == m_aBannedIPs[i])
		{
			// calculate remaining ban time
			timeRemaining = (m_aBanStartTime[i] + m_aBanDuration[i]) - currentTime;
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
	close(psSlotInfo->fdSocket);
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
	int amtSlots = ARRAYSIZE(m_aSuspiciousIPs);
	int addedIP = false;

	// check if the IP is already known
	for (int i = 0; i < amtSlots; ++i)
	{
		if (m_aSuspiciousIPs[i] == IP)
		{
			knownSlotIndex = i;
			break;
		}
	}

	// if slot was found, increase the sus counter
	if (knownSlotIndex != -1)
	{
		m_aSuspiciousAttempts[knownSlotIndex]++;
		addedIP = true;

		// refresh suspicious start time
		m_aSuspiciousStartTime[knownSlotIndex] = time(0);
	}
	else // add the IP to the first free slot
	{
		for (int i = 0; i < amtSlots; ++i)
		{
			if (m_aSuspiciousIPs[i] == 0)
			{
				m_aSuspiciousIPs[i] = IP;
				m_aSuspiciousAttempts[i]++;
				m_aSuspiciousStartTime[i] = time(0);
				addedIP = true;
				break;
			}
		}
	}

	// if no IP matched and all slots full, error
	if (!addedIP)
	{
		m_pMainlogic->m_Log.Log("%s: All suspicious IP slots occupied (%d)", __FUNCTION__, amtSlots);
		return ERROR;
	}

	return OK;
}

void CServer::RemoveSuspiciousIP(in_addr_t IP)
{
	int amtSlots = ARRAYSIZE(m_aSuspiciousIPs);

	// check if the IP is already known
	for (int i = 0; i < amtSlots; ++i)
	{
		if (m_aSuspiciousIPs[i] == IP)
		{
			m_aSuspiciousIPs[i] = 0;
			m_aSuspiciousAttempts[i] = 0;
			m_aSuspiciousStartTime[i] = 0;

			return;
		}
	}
}

int CServer::GetSuspiciousAttempts(in_addr_t IP)
{
	int attempts = 0;

	// check if IP in list
	for (int i = 0; i < ARRAYSIZE(m_aSuspiciousIPs); ++i)
	{
		if (IP == m_aSuspiciousIPs[i])
		{
			attempts = m_aSuspiciousAttempts[i];
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

void CServer::ComHelp(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp)
{
	strncat(pResp, "****** Help ******\n" CSERVER_RESPONSE_PREFIX "All commands are case insensitive.\n", LenResp);

	// if logged in, inform as who
	if (psSlotInfo->loggedIn)
	{
		snprintf(pBufTemp, LenBufTemp, CSERVER_RESPONSE_PREFIX "Logged in as '%s'", psSlotInfo->aUsername);
		strncat(pResp, pBufTemp, LenResp);

		// activated account becomes notified of elevated status
		if (psSlotInfo->activated)
		{
			snprintf(pBufTemp, LenBufTemp, ", your account is activated");
			strncat(pResp, pBufTemp, LenResp);
		}

		// add newlines
		snprintf(pBufTemp, LenBufTemp, ".\n");
		strncat(pResp, pBufTemp, LenResp);
	}

	// add newlines
	snprintf(pBufTemp, LenBufTemp, CSERVER_RESPONSE_PREFIX "\n");
	strncat(pResp, pBufTemp, LenResp);

	// print all commands that the user can see
	for (int i = 0; i < ARRAYSIZE(m_asCommands); ++i)
	{
		if (!IsCommandVisible(psSlotInfo, m_asCommands[i].flags))
			continue;

		snprintf(pBufTemp, LenBufTemp, CSERVER_RESPONSE_PREFIX "%s... %s. Usage: '%s%s%s'.", m_asCommands[i].aName, m_asCommands[i].aHelp, m_asCommands[i].aName, strnlen(m_asCommands[i].aParams, ARRAYSIZE(m_asCommands[0].aParams)) > 0 ? " " : "", m_asCommands[i].aParams);
		strncat(pResp, pBufTemp, LenResp);

		// don't give example if no params needed
		if (strnlen(m_asCommands[i].aExample, ARRAYSIZE(m_asCommands[0].aExample)) > 0)
		{
			snprintf(pBufTemp, LenBufTemp, " Example: '%s %s'\n", m_asCommands[i].aName, m_asCommands[i].aExample);
			strncat(pResp, pBufTemp, LenResp);
		}
		else
		{
			strncat(pResp, "\n", LenResp);
		}
	}

	strncat(pResp, CSERVER_RESPONSE_PREFIX "******************", LenResp);
}

int CServer::ComActivateAccount(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, const char *pPhrase1, const char *pPhrase2, const char *pPhrase3, size_t LenPhrases)
{
	int retval = 0;

	// check if the passphrase is correct
	if (CCore::StringCompareNocase(pPhrase1, CSERVER_COM_ACTIVATE_PASS1, LenPhrases) == 0 &&
		CCore::StringCompareNocase(pPhrase2, CSERVER_COM_ACTIVATE_PASS2, LenPhrases) == 0 &&
		CCore::StringCompareNocase(pPhrase3, CSERVER_COM_ACTIVATE_PASS3, LenPhrases) == 0)
	{
		retval = WriteKey(psSlotInfo, FILETYPE_ACCOUNT, ACCKEY_ACTIVATED, "1", pResp, LenResp);
		if (retval != OK)
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Failed to activate account '%s'", psSlotInfo->slotIndex, psSlotInfo->aUsername);
		}
		else
		{
			psSlotInfo->activated = true;
			strncat(pResp, CSERVER_COM_ACTIVATE_RESPONSE "! Account activated!", LenResp);
			m_pMainlogic->m_Log.Log("Slot[%d] Account '%s' has been activated", psSlotInfo->slotIndex, psSlotInfo->aUsername);
		}
	}
	else
	{
		return ERROR;
	}

	return OK;
}

void CServer::ComRegister(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp, char *pUsername, char *pPassword)
{
	int retval = 0;

	retval = AccountAction(ACCACTION_REGISTER, psSlotInfo, pUsername, pPassword, NULL, pResp, LenResp);
	if (retval != OK)
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Failed to register <%s> <%s>", psSlotInfo->slotIndex, pUsername, pPassword);
	}
	else
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Registered user <%s> <%s>", psSlotInfo->slotIndex, pUsername, pPassword);
		snprintf(pBufTemp, LenBufTemp, "Successfully registered, please log in!");
		strncat(pResp, pBufTemp, LenResp);
	}
}

void CServer::ComLogin(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp, char *pUsername, char *pPassword)
{
	int retval = 0;
	int wrongCredentials = false;
	int susAttempts = 0;
	int doBan = false;
	time_t banTime = 0;
	struct tm sTime = {0};

	retval = AccountAction(ACCACTION_LOGIN, psSlotInfo, pUsername, pPassword, &wrongCredentials, pResp, LenResp);
	if (retval != OK)
	{
		if (wrongCredentials) // if the reason for fail is "wrong credentials"
		{
			psSlotInfo->failedLogins++;
			retval = AddSuspiciousIP(psSlotInfo->clientIP);
			if (retval != OK)
			{
				m_pMainlogic->m_Log.Log("Slot[%d] Unable to add suspicious IP '%s', safety mesure: Exit application", psSlotInfo->slotIndex, inet_ntoa(GetIPStruct(psSlotInfo->clientIP)));
				m_pMainlogic->RequestApplicationExit();
			}
		}

		susAttempts = GetSuspiciousAttempts(psSlotInfo->clientIP);
		m_pMainlogic->m_Log.Log("Slot[%d] Failed to log in with '%s' '%s', failed logins: %d/%d, total attempts of '%s': %d/%d", psSlotInfo->slotIndex, pUsername, pPassword, psSlotInfo->failedLogins, CSERVER_MAX_FAILED_LOGINS, inet_ntoa(GetIPStruct(psSlotInfo->clientIP)), susAttempts, CSERVER_MAX_FAILED_LOGINS_SUSPICIOUS);

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
				m_pMainlogic->m_Log.Log("Slot[%d] Unable to ban IP '%s', safety mesure: Exit application", psSlotInfo->slotIndex, inet_ntoa(GetIPStruct(psSlotInfo->clientIP)));
				m_pMainlogic->RequestApplicationExit();
			}
			else
			{
				psSlotInfo->banned = true;
				sTime = *localtime(&banTime);
				m_pMainlogic->m_Log.Log("Slot[%d] Banned IP '%s' for %dh %dm %ds", psSlotInfo->slotIndex, inet_ntoa(GetIPStruct(psSlotInfo->clientIP)), sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
				snprintf(pBufTemp, LenBufTemp, "\n" CSERVER_RESPONSE_PREFIX "You have been banned for %dh %dm %ds", sTime.tm_hour - 1, sTime.tm_min, sTime.tm_sec);
				strncat(pResp, pBufTemp, LenResp);
			}
		}
	}
	else
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Account '%s' logged in", psSlotInfo->slotIndex, psSlotInfo->aUsername);
		snprintf(pBufTemp, LenBufTemp, "Welcome back %s", pUsername);
		strncat(pResp, pBufTemp, LenResp);

		// activated account becomes notified of elevated status
		if (psSlotInfo->activated)
			snprintf(pBufTemp, LenBufTemp, ", your account is activated!");
		else
			snprintf(pBufTemp, LenBufTemp, "!");

		strncat(pResp, pBufTemp, LenResp);
	}
}

void CServer::ComLogout(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp)
{
	int retval = 0;

	retval = AccountAction(ACCACTION_LOGOUT, psSlotInfo, NULL, NULL, NULL, pResp, LenResp);
	if (retval != OK)
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Failed to log out with account '%s'", psSlotInfo->slotIndex, psSlotInfo->aUsername);
	}
	else
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Account logged out", psSlotInfo->slotIndex);
		snprintf(pBufTemp, LenBufTemp, "Logged out");
		strncat(pResp, pBufTemp, LenResp);
	}
}

void CServer::ComShutdown(S_SLOTINFO *psSlotInfo)
{
	m_pMainlogic->m_Log.Log("Slot[%d] Has shut down the server", psSlotInfo->slotIndex);
	m_pMainlogic->RequestApplicationExit();
}

void CServer::ComRun(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp, const char *pMsgFull)
{
	int retval = 0;
	char aFullMessage[CSERVER_NET_BUFFERSIZE] = {0};
	char aFullParameters[CSERVER_NET_BUFFERSIZE] = {0};
	char *pRest = 0;

	// remove command from parameters
	strncpy(aFullMessage, pMsgFull, ARRAYSIZE(aFullMessage));
	pRest = aFullMessage;
	strtok_r(pRest, " ", &pRest);
	strncpy(aFullParameters, pRest, ARRAYSIZE(aFullParameters));

	m_pMainlogic->m_Log.Log("Slot[%d] Running command '%s'...", psSlotInfo->slotIndex, aFullParameters);

	retval = system(aFullParameters);

	m_pMainlogic->m_Log.Log("Slot[%d] Command returned %d", psSlotInfo->slotIndex, retval);
	snprintf(pBufTemp, LenBufTemp, "'%s' returned %d", aFullParameters, retval);
	strncat(pResp, pBufTemp, LenResp);
}

int CServer::ComDelete(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp, char *pTarget, size_t LenTarget)
{
	int retval = 0;

	// log
	if (CCore::StringCompareNocase(pTarget, "log", LenTarget) == 0)
	{
		retval = RemoveFile(psSlotInfo, FILETYPE_LOG, pResp, LenResp);
		if (retval != OK)
		{
			m_pMainlogic->m_Log.Log("%s: Failed to remove log file", __FUNCTION__);
		}
		else
		{
			strncat(pResp, "Deleted log file", LenResp);
			m_pMainlogic->m_Log.Log("Slot[%d] User '%s' deleted log", psSlotInfo->slotIndex, psSlotInfo->aUsername);
		}
	} // account
	else if (CCore::StringCompareNocase(pTarget, "account", LenTarget) == 0)
	{
		// delete account
		retval = RemoveFile(psSlotInfo, FILETYPE_ACCOUNT, pResp, LenResp);
		if (retval != OK)
		{
			m_pMainlogic->m_Log.Log("%s: Failed to delete account file, defines were not removed", __FUNCTION__);
		}
		else
		{
			// delete defines GPIO
			retval = RemoveFile(psSlotInfo, FILETYPE_DEFINES_GPIO, pResp, LenResp);
			if (retval != OK)
			{
				m_pMainlogic->m_Log.Log("%s: Failed to additionally delete defines GPIO file", __FUNCTION__);
			}
			else
			{
				// delete defines MOSFET
				retval = RemoveFile(psSlotInfo, FILETYPE_DEFINES_MOSFET, pResp, LenResp);
				if (retval != OK)
				{
					m_pMainlogic->m_Log.Log("%s: Failed to additionally delete defines MOSFET file", __FUNCTION__);
				}
				else
				{
					// log out
					retval = AccountAction(ACCACTION_LOGOUT, psSlotInfo, NULL, NULL, NULL, pResp, LenResp);
					if (retval != OK)
					{
						m_pMainlogic->m_Log.Log("Slot[%d] Failed to additionally log out with account '%s'", psSlotInfo->slotIndex, psSlotInfo->aUsername);
					}
					else
					{
						m_pMainlogic->m_Log.Log("Slot[%d] Deleted account and logged out", psSlotInfo->slotIndex);
						snprintf(pBufTemp, LenBufTemp, "Deleted account and logged out");
						strncat(pResp, pBufTemp, LenResp);
					}
				}
			}
		}
	} // defines are just cleared because the file is still needed
	else if (CCore::StringCompareNocase(pTarget, "defines", LenTarget) == 0)
	{
		// clear defines GPIO
		retval = CreateDefinesFile(FILETYPE_DEFINES_GPIO, psSlotInfo->aUsername, pResp, LenResp);
		if (retval != OK)
		{
			m_pMainlogic->m_Log.Log("Slot[%d] User '%s' unable to clear defines GPIO", psSlotInfo->slotIndex, psSlotInfo->aUsername);
		}
		else
		{
			// clear defines MOSFET
			retval = CreateDefinesFile(FILETYPE_DEFINES_MOSFET, psSlotInfo->aUsername, pResp, LenResp);
			if (retval != OK)
			{
				m_pMainlogic->m_Log.Log("Slot[%d] User '%s' unable to clear defines MOSFET", psSlotInfo->slotIndex, psSlotInfo->aUsername);
			}
			else
			{
				m_pMainlogic->m_Log.Log("Slot[%d] User '%s' defines were cleared", psSlotInfo->slotIndex, psSlotInfo->aUsername);
				snprintf(pBufTemp, LenBufTemp, "Deleted defines");
				strncat(pResp, pBufTemp, LenResp);
			}
		}
	} // all
	else if (CCore::StringCompareNocase(pTarget, "all", LenTarget) == 0)
	{
		// safety measure in case we ever change the accounts / defines directory to anywhere that is not our program directory
		if (strstr(CSERVER_FOLDERPATH_ACCOUNTS, FILEPATH_BASE) == 0 || strstr(CSERVER_FOLDERPATH_DEFINES_GPIO, FILEPATH_BASE) == 0 || strstr(CSERVER_FOLDERPATH_DEFINES_MOSFET, FILEPATH_BASE) == 0)
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Danger! Folder path for accounts or defines are not inside the base folder path '%s', important files could be deleted", psSlotInfo->slotIndex, FILEPATH_BASE);
			snprintf(pBufTemp, LenBufTemp, "Unable to delete all files", CSERVER_FOLDERPATH_ACCOUNTS);
			strncat(pResp, pBufTemp, LenResp);
		}
		else
		{
			// remove accounts
			retval = CCore::RemoveFilesDirectory(CSERVER_FOLDERPATH_ACCOUNTS);
			if (retval != OK)
			{
				m_pMainlogic->m_Log.Log("Slot[%d] Failed to remove files in directory '%s'", psSlotInfo->slotIndex, CSERVER_FOLDERPATH_ACCOUNTS);
				snprintf(pBufTemp, LenBufTemp, "Failed to remove files in directory '%s'", CSERVER_FOLDERPATH_ACCOUNTS);
				strncat(pResp, pBufTemp, LenResp);
			}
			else
			{
				// remove defines GPIO
				retval = CCore::RemoveFilesDirectory(CSERVER_FOLDERPATH_DEFINES_GPIO);
				if (retval != OK)
				{
					m_pMainlogic->m_Log.Log("Slot[%d] Failed to remove files in directory '%s'", psSlotInfo->slotIndex, CSERVER_FOLDERPATH_DEFINES_GPIO);
					snprintf(pBufTemp, LenBufTemp, "Failed to remove files in directory '%s'", CSERVER_FOLDERPATH_DEFINES_GPIO);
					strncat(pResp, pBufTemp, LenResp);
				}
				else
				{
					// remove defines MOSFET
					retval = CCore::RemoveFilesDirectory(CSERVER_FOLDERPATH_DEFINES_MOSFET);
					if (retval != OK)
					{
						m_pMainlogic->m_Log.Log("Slot[%d] Failed to remove files in directory '%s'", psSlotInfo->slotIndex, CSERVER_FOLDERPATH_DEFINES_GPIO);
						snprintf(pBufTemp, LenBufTemp, "Failed to remove files in directory '%s'", CSERVER_FOLDERPATH_DEFINES_GPIO);
						strncat(pResp, pBufTemp, LenResp);
					}
					else
					{
						// remove log
						retval = RemoveFile(psSlotInfo, FILETYPE_LOG, pResp, LenResp);
						if (retval != OK)
						{
							m_pMainlogic->m_Log.Log("Slot[%d] Failed to remove log file", psSlotInfo->slotIndex);
						}
						else
						{
							// log out
							retval = AccountAction(ACCACTION_LOGOUT, psSlotInfo, NULL, NULL, NULL, pResp, LenResp);
							if (retval != OK)
							{
								m_pMainlogic->m_Log.Log("Slot[%d] Failed to additionally log out with account '%s'", psSlotInfo->slotIndex, psSlotInfo->aUsername);
							}
							else
							{
								m_pMainlogic->m_Log.Log("Slot[%d] Deleted all files and logged out", psSlotInfo->slotIndex);
								snprintf(pBufTemp, LenBufTemp, "Deleted all files and logged out");
								strncat(pResp, pBufTemp, LenResp);
							}
						}
					}
				}
			}
		}
	}
	else // invalid category
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Wrong parameters for deleting", psSlotInfo->slotIndex);
		return ERROR;
	}

	return OK;
}

void CServer::ComIODefine(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp, char *pNumber, char *pState, size_t LenParams)
{
	int retval = 0;
	int pinNumber = atoi(pNumber);

	if (CCore::IsLetter(pNumber[0]))
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Error, GPIO number must be a number", psSlotInfo->slotIndex);
		snprintf(pBufTemp, LenBufTemp, "Error, GPIO number must be a number");
		strncat(pResp, pBufTemp, LenResp);
	} // number must be in range
	else if (!m_Hardware.IsIOValid(CHardware::GPIO, pinNumber))
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Error, GPIO number must be " CSERVER_COM_GPIO_RANGE, psSlotInfo->slotIndex);
		snprintf(pBufTemp, LenBufTemp, "Error, GPIO number must be " CSERVER_COM_GPIO_RANGE);
		strncat(pResp, pBufTemp, LenResp);
	} // name must begin with letter
	else if (!CCore::IsLetter(pState[0]))
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Error, name must begin with a letter", psSlotInfo->slotIndex);
		snprintf(pBufTemp, LenBufTemp, "Error, name must begin with a letter");
		strncat(pResp, pBufTemp, LenResp);
	} // name must be simple ascii
	else if (!CCore::CheckStringAscii(pState, LenParams))
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Error, name must contain only characters " CSERVER_CHARRANGE_ASCII_READABLE, psSlotInfo->slotIndex);
		snprintf(pBufTemp, LenBufTemp, "Error, name must contain only characters " CSERVER_CHARRANGE_ASCII_READABLE);
		strncat(pResp, pBufTemp, LenResp);
	}
	else
	{
		if (strlen(pState) < CSERVER_MIN_LEN_DEFINES || strlen(pState) > CSERVER_MAX_LEN_DEFINES)
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Error, define name must have %d - %d characters", psSlotInfo->slotIndex, CSERVER_MIN_LEN_DEFINES, CSERVER_MAX_LEN_DEFINES);
			snprintf(pBufTemp, LenBufTemp, "Error, define name must have %d - %d characters", CSERVER_MIN_LEN_DEFINES, CSERVER_MAX_LEN_DEFINES);
			strncat(pResp, pBufTemp, LenResp);
		}
		else if (!CCore::CheckStringAscii(pState, LenParams))
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Error, define name must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE, psSlotInfo->slotIndex);
			snprintf(pBufTemp, LenBufTemp, "Error, define name must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE);
			strncat(pResp, pBufTemp, LenResp);
		}
		else
		{
			retval = WriteKey(psSlotInfo, FILETYPE_DEFINES_GPIO, (E_ACCOUNTKEYS)pinNumber, pState, pResp, LenResp);
			if (retval != OK)
			{
				m_pMainlogic->m_Log.Log("Slot[%d] Failed to define <%d> <%s>", psSlotInfo->slotIndex, pinNumber, pState);
			}
			else
			{
				m_pMainlogic->m_Log.Log("Slot[%d] Defined GPIO %d as '%s'", psSlotInfo->slotIndex, pinNumber, pState);
				snprintf(pBufTemp, LenBufTemp, "Defined GPIO %d as '%s'", pinNumber, pState);
				strncat(pResp, pBufTemp, LenResp);
			}
		}
	}

	/*
		pinNumber = atoi(aaToken[1]);

		if (CCore::IsLetter(aaToken[1][0]))
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Error, MOSFET number must be a number", psSlotInfo->slotIndex);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, MOSFET number must be a number");
			strncat(pResp, aBufTemp, LenResp);
		} // number must be in range
		else if (!m_Hardware.IsIOValid(CHardware::MOSFET, pinNumber))
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Error, MOSFET number must be " CSERVER_COM_MOSFET_RANGE, psSlotInfo->slotIndex);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, MOSFET number must be " CSERVER_COM_MOSFET_RANGE);
			strncat(pResp, aBufTemp, LenResp);
		} // name must begin with letter
		else if (!CCore::IsLetter(aaToken[2][0]))
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Error, name must begin with a letter", psSlotInfo->slotIndex);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name must begin with a letter");
			strncat(pResp, aBufTemp, LenResp);
		} // name must be simple ascii
		else if (!CCore::CheckStringAscii(aaToken[2], ARRAYSIZE(aaToken[0])))
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Error, name must contain only characters " CSERVER_CHARRANGE_ASCII_READABLE, psSlotInfo->slotIndex);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, name must contain only characters " CSERVER_CHARRANGE_ASCII_READABLE);
			strncat(pResp, aBufTemp, LenResp);
		}
		else
		{
			if (strlen(aaToken[2]) < CSERVER_MIN_LEN_DEFINES || strlen(aaToken[2]) > CSERVER_MAX_LEN_DEFINES)
			{
				m_pMainlogic->m_Log.Log("Slot[%d] Error, define name must have %d - %d characters", psSlotInfo->slotIndex, CSERVER_MIN_LEN_DEFINES, CSERVER_MAX_LEN_DEFINES);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, define name must have %d - %d characters", CSERVER_MIN_LEN_DEFINES, CSERVER_MAX_LEN_DEFINES);
				strncat(pResp, aBufTemp, LenResp);
			}
			else if (!CCore::CheckStringAscii(aaToken[2], ARRAYSIZE(aaToken[2])))
			{
				m_pMainlogic->m_Log.Log("Slot[%d] Error, define name must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE, psSlotInfo->slotIndex);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error, define name must only contain the characters " CSERVER_CHARRANGE_ASCII_READABLE);
				strncat(pResp, aBufTemp, LenResp);
			}
			else
			{
				retval = WriteKey(psSlotInfo, FILETYPE_DEFINES_MOSFET, (E_ACCOUNTKEYS)pinNumber, aaToken[2], pResp, LenResp);
				if (retval != OK)
				{
					m_pMainlogic->m_Log.Log("Slot[%d] Failed to define <%d> <%s>", psSlotInfo->slotIndex, pinNumber, aaToken[2]);
				}
				else
				{
					m_pMainlogic->m_Log.Log("Slot[%d] Defined MOSFET %d as '%s'", psSlotInfo->slotIndex, pinNumber, aaToken[2]);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Defined MOSFET %d as '%s'", pinNumber, aaToken[2]);
					strncat(pResp, aBufTemp, LenResp);
				}
			}
		}
	*/
}

void CServer::ComIOSet(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp, char *pNumber, char *pState, size_t LenParams)
{
	int retval = 0;
	int isName = false;
	int pinNumber = -1;
	int pinState = -1;
	char aRead[CSERVER_MAX_LEN_LINES] = {0};

	// check if name is name or number
	if (CCore::IsLetter(pNumber[0]))
		isName = true;

	if (isName)
	{
		for (int i = 0; i <= 31; ++i)
		{
			// skip non-GPIO and i2c-1 pins
			if (!m_Hardware.IsIOValid(CHardware::GPIO, i))
				continue;

			retval = ReadKey(psSlotInfo->aUsername, FILETYPE_DEFINES_GPIO, (E_ACCOUNTKEYS)i, aRead, ARRAYSIZE(aRead), pResp, LenResp);
			if (retval != OK)
			{
				m_pMainlogic->m_Log.Log("Slot[%d] Failed to read key %d", psSlotInfo->slotIndex, i);
			}
			else // compare name
			{
				if (CCore::StringCompareNocase(aRead, pNumber, ARRAYSIZE(aRead)) == 0)
				{
					pinNumber = i;
					break;
				}
			}
		}
	}
	else
	{
		pinNumber = atoi(pNumber);
	}

	// check GPIO number
	if (!m_Hardware.IsIOValid(CHardware::GPIO, pinNumber))
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Error, name not found or GPIO number outside " CSERVER_COM_GPIO_RANGE, psSlotInfo->slotIndex);
		snprintf(pBufTemp, LenBufTemp, "Error, name not found or GPIO number outside " CSERVER_COM_GPIO_RANGE);
		strncat(pResp, pBufTemp, LenResp);
	}
	else
	{
		// check state
		isName = false;
		pinState = -1;

		// check if state is name or number (or empty)
		if (CCore::IsLetter(pState[0]) || strlen(pState) == 0)
			isName = true;

		// check on/off/high/low
		if (isName)
		{
			if (CCore::StringCompareNocase(pState, "on", LenParams) == 0)
				pinState = 1;
			else if (CCore::StringCompareNocase(pState, "off", LenParams) == 0)
				pinState = 0;
			else if (CCore::StringCompareNocase(pState, "high", LenParams) == 0)
				pinState = 1;
			else if (CCore::StringCompareNocase(pState, "low", LenParams) == 0)
				pinState = 0;
		}
		else
		{
			pinState = atoi(pState);
		}

		// check GPIO state validity
		if (pinState < 0 || pinState > 1)
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Error unknown GPIO state '%s'", psSlotInfo->slotIndex, pState);
			snprintf(pBufTemp, LenBufTemp, "Error unknown GPIO state '%s'", pState);
			strncat(pResp, pBufTemp, LenResp);
		}
		else
		{
			// set GPIO
			retval = m_Hardware.SetIO(CHardware::GPIO, pinNumber, (CHardware::E_HWSTATE)pinState);
			if (retval != OK)
			{
				m_pMainlogic->m_Log.Log("Slot[%d] Failed to write GPIO, returned %d", psSlotInfo->slotIndex, retval);
				snprintf(pBufTemp, LenBufTemp, "Failed to write GPIO, returned %d", retval);
				strncat(pResp, pBufTemp, LenResp);
			}
			else
			{
				m_pMainlogic->m_Log.Log("Slot[%d] GPIO %s (%d) --> %s", psSlotInfo->slotIndex, pNumber, pinNumber, pState);
				snprintf(pBufTemp, LenBufTemp, "GPIO %s (%d) --> %s", pNumber, pinNumber, pState);
				strncat(pResp, pBufTemp, LenResp);
			}
		}
	}

	/*
		// check number
		isName = false;
		pinNumber = -1;

		// check if name is name or number
		if (CCore::IsLetter(aaToken[1][0]))
			isName = true;

		if (isName)
		{
			for (int i = 0; i <= 8; ++i)
			{
				retval = ReadKey(psSlotInfo->aUsername, FILETYPE_DEFINES_MOSFET, (E_ACCOUNTKEYS)i, aRead, ARRAYSIZE(aRead), pResp, LenResp);
				if (retval != OK)
				{
					m_pMainlogic->m_Log.Log("Slot[%d] Failed to read key %d", psSlotInfo->slotIndex, i);
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
		if (!m_Hardware.IsIOValid(CHardware::MOSFET, pinNumber))
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Error, name not found or MOSFET number outside " CSERVER_COM_MOSFET_RANGE, psSlotInfo->slotIndex);
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
				m_pMainlogic->m_Log.Log("Slot[%d] Error unknown MOSFET state '%s'", psSlotInfo->slotIndex, aaToken[2]);
				snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Error unknown MOSFET state '%s'", aaToken[2]);
				strncat(pResp, aBufTemp, LenResp);
			}
			else
			{
				// set MOSFET
				retval = m_Hardware.SetIO(CHardware::MOSFET, pinNumber, (CHardware::E_HWSTATE)pinState);
				if (retval != OK)
				{
					m_pMainlogic->m_Log.Log("Slot[%d] Failed to write MOSFET, returned %d", psSlotInfo->slotIndex, retval);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to write MOSFET, returned %d", retval);
					strncat(pResp, aBufTemp, LenResp);
				}
				else
				{
					m_pMainlogic->m_Log.Log("Slot[%d] MOSFET %s (%d) --> %s", psSlotInfo->slotIndex, aaToken[1], pinNumber, aaToken[2]);
					snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "MOSFET %s (%d) --> %s", aaToken[1], pinNumber, aaToken[2]);
					strncat(pResp, aBufTemp, LenResp);
				}
			}
		}
	*/
}

void CServer::ComIORead(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp)
{
}

void CServer::ComIOClear(S_SLOTINFO *psSlotInfo, char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp)
{
	int retval = 0;

	retval = m_Hardware.ClearIO(CHardware::GPIO);
	if (retval != OK)
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Failed to clear GPIOs, returned %d", psSlotInfo->slotIndex, retval);
		snprintf(pBufTemp, LenBufTemp, "Failed to clear GPIOs, returned %d", retval);
		strncat(pResp, pBufTemp, LenResp);
	}
	else
	{
		m_pMainlogic->m_Log.Log("Slot[%d] Cleared GPIOs", psSlotInfo->slotIndex);
		snprintf(pBufTemp, LenBufTemp, "Cleared GPIOs");
		strncat(pResp, pBufTemp, LenResp);
	}

	/*
		retval = m_Hardware.ClearIO(CHardware::MOSFET);
		if (retval != OK)
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Failed to clear MOSFETs, returned %d", psSlotInfo->slotIndex, retval);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Failed to clear MOSFETs, returned %d", retval);
			strncat(pResp, aBufTemp, LenResp);
		}
		else
		{
			m_pMainlogic->m_Log.Log("Slot[%d] Cleared MOSFETs", psSlotInfo->slotIndex);
			snprintf(aBufTemp, ARRAYSIZE(aBufTemp), "Cleared MOSFETs");
			strncat(pResp, aBufTemp, LenResp);
		}
	*/
}

void CServer::ComEcho(char *pResp, size_t LenResp, char *pBufTemp, size_t LenBufTemp)
{
	int randNr = rand() % ARRAYSIZE(m_aaEchoes);

	snprintf(pBufTemp, LenBufTemp, "You shouted... you hear '%s'", m_aaEchoes[randNr]);
	strncat(pResp, pBufTemp, LenResp);
	m_pMainlogic->m_Log.Log("Echoed '%s'", pResp);
}

void CServer::ComExit(S_SLOTINFO *psSlotInfo)
{
	psSlotInfo->requestedDisconnect = true;
}