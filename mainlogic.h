#pragma once

#include <thread>

#include "main.h"
#include "server.h"
#include "log.h"

#define CMAINLOGIC_DELAY_MAINLOOP 100000            // 100ms
#define CMAINLOGIC_DELAY_KEYBOARDCONTROLLOOP 100000 // 100ms

#define CMAINLOGIC_KBDKEY_ENDPROGRAM 'e'

// enums
typedef enum
{
    THR_SERVERRUN,
    THR_KEYBOARDCONTROL,
    AMOUNT_THREADS,
} E_THREADS;

class CServer;

class CMainlogic
{
public:
    CMainlogic();
    ~CMainlogic();
    int EntryPoint();
    int SetThreadStatus(E_THREADS Thread, int Status);
    void RequestApplicationExit();
    CLog *m_pLog;

private:
    int Keyboardcontrol();
    int ExitApplication();

    std::thread m_aThread[AMOUNT_THREADS];
    int m_aThreadStatus[AMOUNT_THREADS];
    bool m_DoExitApplication;
    CServer *m_pServer;
};