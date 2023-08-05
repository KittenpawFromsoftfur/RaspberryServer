#pragma once

#define CHARDWARE_MAX_LEN_SYSTEMCOMMAND 256

// classes
class CHardware
{
public:
    static void Init();
    static void SetGpio(int Pin, int State);
    static void ClearGpio();
    static bool IsGpioValid(int Number);
    static int SetMosfet(int Pin, int State);
    static int ClearMosfet();
    static bool IsMosfetValid(int Number);
};