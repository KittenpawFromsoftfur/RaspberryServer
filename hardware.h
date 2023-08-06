#pragma once

#define CHARDWARE_MAX_LEN_SYSTEMCOMMAND 256

// classes
class CHardware
{
public:
    CHardware();
    void SetGpio(int Pin, int State);
    int SetMosfet(int Pin, int State);
    void ClearGpio();
    int ClearMosfet();
    bool IsGpioValid(int Number);
    bool IsMosfetValid(int Number);
};