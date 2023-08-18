/**
 * @file mainlogic.h
 * @author KittenpawFromsoftfur (finbox.entertainment@gmail.com)
 * @brief Main logic of the program, starts all threads, handles keyboard control and handles application exit
 * @version 1.0
 * @date 2023-08-18
 *
 * @copyright Copyright (c) 2023
 */
#pragma once

#include <thread>

#include "main.h"
#include "server.h"
#include "log.h"

#define CMAINLOGIC_DELAY_MAINLOOP 100000            // 100ms
#define CMAINLOGIC_DELAY_KEYBOARDCONTROLLOOP 100000 // 100ms

#define CMAINLOGIC_KBDKEY_ENDPROGRAM 'e'
#define CMAINLOGIC_DESCRIPTION_ENDPROGRAM "Exit application"

// classes
class CServer;

class CMainlogic
{
public:
    enum E_THREADS
    {
        THR_SERVERRUN,
        THR_KEYBOARDCONTROL,
        AMOUNT_THREADS,
    };

    enum E_THREADSTATUS
    {
        THS_RUNNING,
        THS_NOTREQUIRED,
        THS_FAILED,
        AMOUNT_THREADSTATUS,
    };

    CMainlogic(const char *pLogFolder, const char *pLogName, int ServerPort, bool UppercaseResponse);
    ~CMainlogic();
    int EntryPoint();
    int SetThreadStatus(E_THREADS ThreadIndex, E_THREADSTATUS Status);
    void RequestApplicationExit();
    CLog m_Log;

private:
    int Keyboardcontrol();
    int ExitApplication();

    bool m_DoExitApplication;
    E_THREADSTATUS m_aThreadStatus[AMOUNT_THREADS];
    std::thread m_aThread[AMOUNT_THREADS];
    CServer *m_pServer;
};