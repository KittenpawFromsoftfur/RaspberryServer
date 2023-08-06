#include <unistd.h>

#include "mainlogic.h"

CMainlogic::CMainlogic()
{
    int m_aThreadStatus = {0};
    bool m_DoExitApplication = false;

    CServer *m_pServer = new CServer(this);
    CLog *m_pLog = new CLog(FOLDERPATH_LOG, LOG_NAME);
}

CMainlogic::~CMainlogic()
{
    delete m_pServer;
    delete m_pLog;
}

int CMainlogic::EntryPoint()
{
    int retval = 0;
    int hasError = false;

    printf("\nhelloing...\n");
    m_pLog->Log("hello");
    usleep(1000000);

    // run server thread
    SetThreadStatus(THR_SERVERRUN, OK);
    m_aThread[THR_SERVERRUN] = std::thread(&CServer::Run, m_pServer);

    // start keyboard control thread
    SetThreadStatus(THR_KEYBOARDCONTROL, OK);
    m_aThread[THR_KEYBOARDCONTROL] = std::thread(&CMainlogic::Keyboardcontrol, this);

    // main loop
    while (1)
    {
        // check thread status
        for (int i = 0; i < ARRAYSIZE(m_aThread); ++i)
        {
            if (m_aThreadStatus[i] != OK)
            {
                m_pLog->Log("%s: There was an error in thread %d, ending...", __FUNCTION__, i);
                hasError = true;
                break;
            }
        }

        // check if application should exit
        if (hasError || m_DoExitApplication)
        {
            m_pLog->Log("Exiting application...");

            retval = ExitApplication();
            if (retval != OK)
                return ERROR;

            break;
        }

        usleep(CMAINLOGIC_DELAY_MAINLOOP);
    }

    m_pLog->Log("Application ended");

    if (hasError)
        return ERROR;

    return OK;
}

int CMainlogic::Keyboardcontrol()
{
    char ch = 0;

    // print usage infos
    printf("\n******************************");
    printf("\n%c... Exit application", CMAINLOGIC_KBDKEY_ENDPROGRAM);
    printf("\n******************************\n\n");

    // process keys
    while (1)
    {
        // keyboard input
        ch = getchar();

        if (ch == CMAINLOGIC_KBDKEY_ENDPROGRAM)
            RequestApplicationExit();

        usleep(CMAINLOGIC_DELAY_KEYBOARDCONTROLLOOP);
    }

    return OK;
}

int CMainlogic::ExitApplication()
{
    int retval = 0;
    int hasError = false;

    // exit server
    retval = m_pServer->OnExitApplication();
    if (retval != OK)
        hasError = true;

    // cancel main threads
    for (int i = 0; i < ARRAYSIZE(m_aThread); ++i)
    {
        retval = CCore::DetachThreadSafely(&m_aThread[i]);
        if (retval != OK)
        {
            m_pLog->Log("%s: Failed to detach thread %d", __FUNCTION__, i);
            hasError = true;
        }
    }

    if (hasError)
        return ERROR;

    return OK;
}

int CMainlogic::SetThreadStatus(E_THREADS Thread, int Status)
{
    if ((Thread < 0 || Thread >= AMOUNT_THREADS) || (Status != OK && Status != ERROR))
    {
        m_pLog->Log("%s: Failed to set thread status [%d] to %d\n", Thread, Status);
        return ERROR;
    }

    m_aThreadStatus[Thread] = Status;

    return OK;
}

void CMainlogic::RequestApplicationExit()
{
    m_DoExitApplication = true;
}