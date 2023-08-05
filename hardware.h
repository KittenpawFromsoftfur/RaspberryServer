#pragma once

// classes
class CHardware
{
public:
    static void Init();
    static void Set(int Pin, int State);
    static void Clear();
    static bool IsGpioValid(int Number);
    static bool IsMosfetValid(int Number);
};