#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>

#include "core.h"

// server
#define NET_SERVER_IP "127.0.0.1"
#define NET_SERVER_PORT 8000
#define NET_BUFFERSIZE 2048

#define MAX_LEN_LISTENQUEUE 10
#define MAX_SLOTS 10

#define FOLDERPATH_FILESTASH_MINIMUM "/home/black/server/"
#define FOLDERPATH_ACCOUNTS "/home/black/server/accounts"
#define FOLDERPATH_DEFINES_GPIO "/home/black/server/defines_gpio"
#define FOLDERPATH_DEFINES_MOSFET "/home/black/server/defines_mosfet"

#define RESPONSE_PREFIX "> "
#define COMMAND_STRING_HELP "help"

#define CHARRANGE_ASCII_READABLE "[Aa-Zz, 0-9]"

#define MAX_LEN_ECHO 128

#define DELAY_RECEIVE 10000
#define DELAY_UPDATELOOP 1000000

#define TIMEOUT_CLIENTCONNECTION_READ 5// s

#define MAX_LEN_TOKEN 256
#define MAX_TOKENS 10

#define MAX_LEN_COMHELP 256
#define MAX_LEN_COMPARAMS 256
#define MAX_LEN_COMEXAMPLE 256

#define MAX_LEN_SYSTEMCOMMAND 256

#define MIN_LEN_CREDENTIALS 1
#define MAX_LEN_CREDENTIALS 16
#define MAX_ACCOUNTS 100
#define MAX_LINES 50
#define MAX_LEN_LINES 64
#define MIN_LEN_DEFINES 1
#define MAX_LEN_DEFINES 16
#define MAX_LEN_USERFILES (MAX_LEN_CREDENTIALS + 2)

#define COMFLAG_EXEC_LOGGEDIN (1 << 0)
#define COMFLAG_EXEC_LOGGEDOUT (1 << 1)
#define COMFLAG_EXEC_ACTIVATEDONLY (1 << 2)// overwrites logged in / out. Incorrect parameters treat as unknown.
#define COMFLAG_VISI_LOGGEDIN (1 << 3)
#define COMFLAG_VISI_LOGGEDOUT (1 << 4)
#define COMFLAG_VISI_ACTIVATEDONLY (1 << 5)// overwrites logged in / out. Incorrect parameters treat as unknown.

#define RESPONSE_LOGIN_FAILED "Wrong username or password"
#define MAX_BANNED_IPS 100
#define MAX_FAILED_LOGINS 3
#define IP_BAN_DURATION_FAILEDLOGINS TIME_TO_SECONDS_HMS(0, 5, 0)
#define MAX_SUSPICIOUS_IPS MAX_BANNED_IPS
#define MAX_FAILED_LOGINS_SUSPICIOUS (MAX_FAILED_LOGINS * 2)// Prevents abuse of failed login counter reset when reconnecting. Forgives clumsiness but also bans abusers harder.
#define IP_BAN_DURATION_SUSPICIOUS TIME_TO_SECONDS_HMS(0, 30, 0)
#define SUSPICIOUS_FALLOFF_TIME TIME_TO_SECONDS_HMS(0, 30, 0)// Is also reset when a ban ends or a login with an activated account succeeds

// command specific
#define COM_GPIO_RANGE "0 - 7, 10 - 16, 21 - 31"
#define COM_MOSFET_RANGE "1 - 8"

#define COM_ACTIVATE_PASS1 "give"
#define COM_ACTIVATE_PASS2 "duck"
#define COM_ACTIVATE_PASS3 "cookie"
#define COM_ACTIVATE_RESPONSE "Quack"

// enums
typedef enum
{
	COM_HELP,
	COM_ACTIVATEACCOUNT,
	COM_REGISTER,
	COM_LOGIN,
	COM_LOGOUT,
	COM_SHUTDOWN,
	COM_RUN,
	COM_DELETE,
	COM_DEFINE,
	COM_SET,
	COM_CLEAR,
	COM_MOSDEFINE,
	COM_MOSSET,
	COM_MOSREAD,
	COM_MOSCLEAR,
	COM_ECHO,
	COM_EXIT,
}E_COMMANDS;

typedef enum
{
	ACCACTION_REGISTER,
	ACCACTION_LOGIN,
	ACCACTION_LOGOUT,
}E_ACCOUNTACTIONS;

typedef enum
{
	ACCKEY_USERNAME,
	ACCKEY_PASSWORD,
	ACCKEY_ACTIVATED,
}E_ACCOUNTKEYS;

typedef enum
{
	FILETYPE_ACCOUNT,
	FILETYPE_DEFINES_GPIO,
	FILETYPE_DEFINES_MOSFET,
	FILETYPE_LOG,
}E_FILETYPES;

// structs
typedef struct
{
	int sockfd;
	int slotIndex;
}S_PARAMS_CLIENTCONNECTION;

typedef struct
{
	E_COMMANDS ID;
	char aName[MAX_LEN_TOKEN];
	char aHelp[MAX_LEN_COMHELP];
	char aParams[MAX_LEN_COMPARAMS];
	char aExample[MAX_LEN_COMEXAMPLE];
	int flags;
}S_COMMAND;

typedef struct
{
	int connfd;
	pthread_t thrClientConnection;
	S_PARAMS_CLIENTCONNECTION threadParamsClientConnection;
	in_addr_t clientIP;
	char aUsername[MAX_LEN_USERFILES];
	int loggedIn;
	int activated;
	int failedLogins;
	int requestedDisconnect;
	int banned;
}S_SLOTINFO;

typedef struct
{
	int listenfd;
	pthread_t thrUpdate;
	in_addr_t aBannedIPs[MAX_BANNED_IPS];
	time_t aBanStartTime[MAX_BANNED_IPS];
	time_t aBanDuration[MAX_BANNED_IPS];
	in_addr_t aSuspiciousIPs[MAX_SUSPICIOUS_IPS];
	int aSuspiciousAttempts[MAX_SUSPICIOUS_IPS];
	time_t aSuspiciousStartTime[MAX_SUSPICIOUS_IPS];
}S_SERVERINFO;

// public functions
void *server_ThrfRun(void *pArgs);
void *server_ThrfOnExitApplication(void *pArgs);
int server_IsGpioValid(int PinNumber);

#endif