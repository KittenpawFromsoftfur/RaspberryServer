#pragma once

#define CHARDWARE_MAX_LEN_SYSTEMCOMMAND 256

// classes
class CHardware
{
public:
    void Init();
    void SetGpio(int Pin, int State);
    void ClearGpio();
    bool IsGpioValid(int Number);
    int SetMosfet(int Pin, int State);
    int ClearMosfet();
    bool IsMosfetValid(int Number);
};