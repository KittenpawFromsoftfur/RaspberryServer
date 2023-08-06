#include <wiringPi.h>
#include <stdlib.h>

#include "core.h"
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

void CHardware::SetGpio(int Pin, int State)
{
	digitalWrite(Pin, State);
}

void CHardware::ClearGpio()
{
	int i = 0;

	// clear all pins
	for (i = 0; i < 32; ++i)
	{
		// skip non-gpio and i2c-1 pins
		if (!IsGpioValid(i))
			continue;

		SetGpio(i, LOW);
	}
}

int CHardware::SetMosfet(int Pin, int State)
{
	int retval = 0;
	char aSystemString[CHARDWARE_MAX_LEN_SYSTEMCOMMAND] = {0};

	snprintf(aSystemString, ARRAYSIZE(aSystemString), "8mosind 0 write %d %s", Pin, State == 1 ? "on" : "off");

	retval = system(aSystemString);

	if (retval != 0)
		return ERROR;

	return OK;
}

int CHardware::ClearMosfet()
{
	int retval = 0;

	retval = system("8mosind 0 write 0");

	if (retval != 0)
		return ERROR;

	return OK;
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