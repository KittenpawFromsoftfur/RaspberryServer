#pragma once

#define CHARDWARE_MAX_LEN_SYSTEMCOMMAND 256

// classes
class CHardware
{
public:
    enum E_HWTYPE
    {
        GPIO,
        MOSFET,
        INVALID,
    };

    enum E_HWSTATE
    {
        OFF = 0,
        ON = 1,
    };

    CHardware();
    bool IsIOValid(E_HWTYPE Type, int IONumber);
    int SetIO(E_HWTYPE Type, int IONumber, E_HWSTATE State);
    int ClearIO(E_HWTYPE Type);
};