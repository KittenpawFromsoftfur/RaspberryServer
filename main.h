#pragma once

#include "core.h"

#define MAX_LEN_FILEPATH 256
#define FILEPATH_BASE "/home/black/server"
#define LOG_FOLDERPATH FILEPATH_BASE "/log"
#define LOG_NAME "_log.txt"

// program values
#define SERVER_PORT_DEFAULT 8000

// main parameters
#define MAX_LEN_MAINPARAMS 256
#define HEADER_LINE "******************************"
#define PREFIX_MAINPARAM "-"
#define MAINPARAM_PORT PREFIX_MAINPARAM "p"              // sets the port of the server
#define MAINPARAM_UPPERCASERESPONSE PREFIX_MAINPARAM "u" // sets the uppercase response of the server
#define DESCRIPTION_PARAM_PORT "Server port (optional, default " STRINGIFY_VALUE(SERVER_PORT_DEFAULT) ")"
#define DESCRIPTION_PARAM_UPPERCASERESPONSE "Enable uppercase response (optional, default 0)"