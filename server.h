#pragma once

#include <arpa/inet.h>
#include <time.h>
#include <thread>

#include "core.h"
#include "main.h"
#include "log.h"
#include "hardware.h"
#include "mainlogic.h"

// server
#define CSERVER_NET_PORT 8000
#define CSERVER_NET_BUFFERSIZE 2048

#define CSERVER_MAX_LEN_LISTENQUEUE 10
#define CSERVER_MAX_SLOTS 10

#define CSERVER_FOLDERPATH_ACCOUNTS FILEPATH_BASE "/accounts"
#define CSERVER_FOLDERPATH_DEFINES_GPIO FILEPATH_BASE "/defines_gpio"
#define CSERVER_FOLDERPATH_DEFINES_MOSFET FILEPATH_BASE "/defines_mosfet"

#define CSERVER_RESPONSE_PREFIX "> "
#define CSERVER_COMMAND_STRING_HELP "help"

#define CSERVER_CHARRANGE_ASCII_READABLE "[Aa-Zz, 0-9]"

#define CSERVER_MAX_ECHOES 25
#define CSERVER_MAX_LEN_ECHO 128

#define CSERVER_DELAY_RECEIVE 10000		 // 10ms
#define CSERVER_DELAY_UPDATELOOP 1000000 // 1s

#define CSERVER_TIMEOUT_CLIENTCONNECTION_READ 5 // s

#define CSERVER_MAX_LEN_TOKEN 256
#define CSERVER_MAX_TOKENS 10

#define CSERVER_MAX_LEN_COMHELP 256
#define CSERVER_MAX_LEN_COMPARAMS 256
#define CSERVER_MAX_LEN_COMEXAMPLE 256

#define CSERVER_MAX_LEN_SYSTEMCOMMAND 256

#define CSERVER_MIN_LEN_CREDENTIALS 1
#define CSERVER_MAX_LEN_CREDENTIALS 16
#define CSERVER_MAX_ACCOUNTS 100
#define CSERVER_MAX_LINES 50
#define CSERVER_MAX_LEN_LINES 64
#define CSERVER_MIN_LEN_DEFINES 1
#define CSERVER_MAX_LEN_DEFINES 16
#define CSERVER_MAX_LEN_USERFILES (CSERVER_MAX_LEN_CREDENTIALS + 2)

#define CSERVER_COMFLAG_EXEC_LOGGEDIN (1 << 0)
#define CSERVER_COMFLAG_EXEC_LOGGEDOUT (1 << 1)
#define CSERVER_COMFLAG_EXEC_ACTIVATEDONLY (1 << 2) // overwrites logged in / out. Incorrect parameters treat as unknown.
#define CSERVER_COMFLAG_VISI_LOGGEDIN (1 << 3)
#define CSERVER_COMFLAG_VISI_LOGGEDOUT (1 << 4)
#define CSERVER_COMFLAG_VISI_ACTIVATEDONLY (1 << 5) // overwrites logged in / out. Incorrect parameters treat as unknown.

#define CSERVER_RESPONSE_LOGIN_FAILED "Wrong username or password"
#define CSERVER_MAX_BANNED_IPS 100
#define CSERVER_MAX_FAILED_LOGINS 3
#define CSERVER_IP_BAN_DURATION_FAILEDLOGINS TIME_TO_SECONDS_HMS(0, 5, 0)
#define CSERVER_MAX_SUSPICIOUS_IPS CSERVER_MAX_BANNED_IPS
#define CSERVER_MAX_FAILED_LOGINS_SUSPICIOUS (CSERVER_MAX_FAILED_LOGINS * 2) // Prevents abuse of failed login counter reset when reconnecting. Forgives clumsiness but also bans abusers harder.
#define CSERVER_IP_BAN_DURATION_SUSPICIOUS TIME_TO_SECONDS_HMS(0, 30, 0)
#define CSERVER_SUSPICIOUS_FALLOFF_TIME TIME_TO_SECONDS_HMS(0, 30, 0) // Is also reset when a ban ends or a login with an activated account succeeds

// command specific
#define CSERVER_COM_GPIO_RANGE "0 - 7, 10 - 16, 21 - 31"
#define CSERVER_COM_MOSFET_RANGE "1 - 8"

#define CSERVER_COM_ACTIVATE_PASS1 "give"
#define CSERVER_COM_ACTIVATE_PASS2 "duck"
#define CSERVER_COM_ACTIVATE_PASS3 "cookie"
#define CSERVER_COM_ACTIVATE_RESPONSE "Quack"

// classes
class CMainlogic;

class CServer
{
public:
	CServer(CMainlogic *pMainlogic);
	~CServer();
	int Run();
	int OnExitApplication();

private:
	enum E_COMMANDS
	{
		COM_HELP,
		COM_ACTIVATEACCOUNT,
		COM_REGISTER,
		COM_LOGIN,
		COM_LOGOUT,
		COM_SHUTDOWN,
		COM_RUN,
		COM_DELETE,
		COM_DEFINE_GPIO,
		COM_SET,
		COM_CLEAR,
		COM_DEFINE_MOSFET,
		COM_MOSSET,
		COM_MOSREAD,
		COM_MOSCLEAR,
		COM_ECHO,
		COM_EXIT,
		AMOUNT_COMMANDS,
	};

	enum E_ACCOUNTACTIONS
	{
		ACCACTION_REGISTER,
		ACCACTION_LOGIN,
		ACCACTION_LOGOUT,
	};

	enum
	{
		ACCKEY_USERNAME,
		ACCKEY_PASSWORD,
		ACCKEY_ACTIVATED,
	};

	enum E_FILETYPES
	{
		FILETYPE_ACCOUNT,
		FILETYPE_DEFINES_GPIO,
		FILETYPE_DEFINES_MOSFET,
		FILETYPE_LOG,
	};

	typedef struct
	{
		int sockfd;
		int slotIndex;
	} S_PARAMS_CLIENTCONNECTION;

	typedef struct
	{
		E_COMMANDS ID;
		char aName[CSERVER_MAX_LEN_TOKEN];
		char aHelp[CSERVER_MAX_LEN_COMHELP];
		char aParams[CSERVER_MAX_LEN_COMPARAMS];
		char aExample[CSERVER_MAX_LEN_COMEXAMPLE];
		int flags;
	} S_COMMAND;

	typedef struct
	{
		int connfd;
		std::thread thrClientConnection;
		S_PARAMS_CLIENTCONNECTION threadParamsClientConnection;
		in_addr_t clientIP;
		char aUsername[CSERVER_MAX_LEN_USERFILES];
		int loggedIn;
		int activated;
		int failedLogins;
		int requestedDisconnect;
		int banned;
	} S_SLOTINFO;

	typedef struct
	{
		int listenfd;
		std::thread thrUpdate;
		in_addr_t aBannedIPs[CSERVER_MAX_BANNED_IPS];
		time_t aBanStartTime[CSERVER_MAX_BANNED_IPS];
		time_t aBanDuration[CSERVER_MAX_BANNED_IPS];
		in_addr_t aSuspiciousIPs[CSERVER_MAX_SUSPICIOUS_IPS];
		int aSuspiciousAttempts[CSERVER_MAX_SUSPICIOUS_IPS];
		time_t aSuspiciousStartTime[CSERVER_MAX_SUSPICIOUS_IPS];
	} S_SERVERINFO;

	int ClientConnection(S_PARAMS_CLIENTCONNECTION *psParams);
	int Update();
	int ParseMessage(S_PARAMS_CLIENTCONNECTION *psParams, const char *pMsg, char *pResp, size_t LenResp);
	void EvaluateTokens(S_PARAMS_CLIENTCONNECTION *psParams, char aaToken[CSERVER_MAX_TOKENS][CSERVER_MAX_LEN_TOKEN], char *pResp, size_t LenResp, const char *pMsgFull);
	int IsCommandExecutable(S_SLOTINFO *psSlotInfo, int Flags);
	int IsCommandVisible(S_SLOTINFO *psSlotInfo, int Flags);
	int ResetHardware();
	void MakeFilepath(E_FILETYPES FileType, const char *pUsername, int FilenameOnly, char *pFullpath, size_t LenFullpath);
	int AccountAction(E_ACCOUNTACTIONS AccountAction, S_SLOTINFO *psSlotInfo, const char *pUsername, const char *pPyword, int *pWrongCredentials, char *pError, size_t LenError);
	int CreateDefinesFile(E_FILETYPES FileType, const char *pUsername, char *pError, size_t LenError);
	int WriteKey(S_SLOTINFO *psSlotInfo, E_FILETYPES FileType, int Key, const char *pValue, char *pError, size_t LenError);
	int ReadKey(const char *pUsername, E_FILETYPES FileType, int Key, char *pKey, size_t LenKey, char *pError, size_t LenError);
	int RemoveFile(S_SLOTINFO *psSlotInfo, E_FILETYPES FileType, char *pError, size_t LenError);
	int BanIP(in_addr_t IP, time_t Duration);
	int CheckIPBanned(S_SLOTINFO *psSlotInfo, struct tm *psTime);
	void CleanOldSession(S_SLOTINFO *psSlotInfo);
	void CloseClientSocket(S_SLOTINFO *psSlotInfo);
	void ResetSlot(S_SLOTINFO *psSlotInfo);
	int AddSuspiciousIP(in_addr_t IP);
	void RemoveSuspiciousIP(in_addr_t IP);
	int GetSuspiciousAttempts(in_addr_t IP);
	struct in_addr GetIPStruct(in_addr_t IP);

	CMainlogic *m_pMainlogic;
	CHardware m_Hardware;
	S_SERVERINFO m_sServerInfo;
	S_SLOTINFO m_asSlotInfo[CSERVER_MAX_SLOTS];
	S_COMMAND m_asCommands[AMOUNT_COMMANDS] =
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
	char m_aaEchoes[CSERVER_MAX_ECHOES][CSERVER_MAX_LEN_ECHO] =
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
};