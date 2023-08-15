#include <wiringPi.h>
#include <stdlib.h>

#include "core.h"
#include "hardware.h"

/* NOTE
	With "gpio readall" you can see the wiringPi numbering scheme
*/

CHardware::CHardware()
{
	wiringPiSetup();

	// set all pins as output
	for (int i = 0; i < 32; ++i)
	{
		// skip non-gpio and i2c-1 pins
		if (!IsIOValid(GPIO, i))
			continue;

		pinMode(i, OUTPUT);
	}
}

bool CHardware::IsIOValid(E_HWTYPE Type, int IONumber)
{
	switch (Type)
	{
	case GPIO:
		if (IONumber < 0 || (IONumber > 7 && IONumber < 10) || (IONumber > 16 && IONumber < 21) || IONumber > 31)
			return false;
		break;

	case MOSFET:
		if (IONumber < 1 || IONumber > 8)
			return false;
		break;
	}

	return true;
}

int CHardware::SetIO(E_HWTYPE Type, int IONumber, E_HWSTATE State)
{
	int retval = 0;
	char aSystemString[CHARDWARE_MAX_LEN_SYSTEMCOMMAND] = {0};

	switch (Type)
	{
	case GPIO:
		digitalWrite(IONumber, State);
		break;

	case MOSFET:
		snprintf(aSystemString, ARRAYSIZE(aSystemString), "8mosind 0 write %d %s", IONumber, State == ON ? "on" : "off");
		retval = system(aSystemString);

		if (retval != 0)
			return ERROR;
		break;
	}

	return OK;
}

int CHardware::ClearIO(E_HWTYPE Type)
{
	int retval = 0;

	switch (Type)
	{
	case GPIO:
		for (int i = 0; i < 32; ++i)
		{
			// skip non-gpio and i2c-1 pins
			if (!IsIOValid(GPIO, i))
				continue;

			SetIO(GPIO, i, OFF);
		}
		break;

	case MOSFET:
		retval = system("8mosind 0 write 0");

		if (retval != 0)
			return ERROR;
		break;
	}

	return OK;
}