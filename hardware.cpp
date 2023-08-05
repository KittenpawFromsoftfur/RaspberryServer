#include <wiringPi.h>

#include "core.h"
#include "log.h"
#include "server.h"
#include "hardware.h"

/* NOTE
	With "gpio readall" you can see the wiringPi numbering scheme
*/

void CHardware::Init()
{
	int i = 0;

	wiringPiSetup();

	// set all pins as output
	for (i = 0; i < 32; ++i)
	{
		// skip non-gpio and i2c-1 pins
		if (!IsGpioValid(i))
			continue;

		pinMode(i, OUTPUT);
	}
}

void CHardware::Set(int Pin, int State)
{
	digitalWrite(Pin, State);
}

void CHardware::Clear()
{
	int i = 0;

	// clear all pins
	for (i = 0; i < 32; ++i)
	{
		// skip non-gpio and i2c-1 pins
		if (!IsGpioValid(i))
			continue;

		Set(i, LOW);
	}
}

bool CHardware::IsGpioValid(int Number)
{
	if (Number < 0 || (Number > 7 && Number < 10) || (Number > 16 && Number < 21) || Number > 31)
		return false;

	return true;
}

bool CHardware::IsMosfetValid(int Number)
{
	if (Number < 1 || Number > 8)
		return false;

	return true;
}