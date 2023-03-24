#include <wiringPi.h>

#include "core.h"
#include "log.h"
#include "server.h"
#include "hardware.h"

/* NOTE
	With "gpio readall" you can see the wiringPi numbering scheme
*/

void hardware_Init()
{
	int i = 0;
	
	wiringPiSetup();
	
	// set all pins as output
	for (i = 0; i < 32; ++i)
	{
		// skip non-gpio and i2c-1 pins
		if (!server_IsGpioValid(i))
			continue;
		
		pinMode(i, OUTPUT);
	}
}

void hardware_Set(int Pin, int State)
{
	digitalWrite(Pin, State);
}

void hardware_Clear()
{
	int i = 0;
	
	// clear all pins
	for (i = 0; i < 32; ++i)
	{
		// skip non-gpio and i2c-1 pins
		if (!server_IsGpioValid(i))
			continue;
		
		hardware_Set(i, LOW);
	}
}