#include "main.h"
#include "mainlogic.h"

/* TODO
	GENERAL
			log thema wie oft wird constructor aufgerufen
			main konstruktor machen
			make mainlogic subclasses initializable via parameters taht are passed in main
		Port to C++ and modularize completely
		split main and cmain (rename)
		CServer structs in klassen splitten und in Funktionen aufteilen
		set/mosset --> set mosfet/gpio ; dasselbe mit allen kommandos...
		Clean up everything / Sort functions properly / Code formatter { 0 }
		rename mainlogic --> cmain
		main() with args so Port can be set from command line etc...
		Server has many functions with pointers to own members --> pass only slotindex
		Unused variables
		config file?

		ENDEND
			Each class has to initialize members to 0 in constructor
			threads have to be detached
			Sort functions

		Test every single function again
		What happens to CPU if we launch per autostart?

	GPIO
		set pinmode
		read pin
		set/read multiple pins at once

	MOSFET
		Implement function "mosread"
		More dynamic setting 0xFF

	INTERFACE
		Make it so you can connect with a browser and click on buttons instead of sending commands

	END
		Documentation please!
*/

int main()
{
	int retval = 0;
	CMainlogic mainlogic;

	retval = mainlogic.EntryPoint();
	mainlogic.m_pLog->Log("Mainlogic ended with return value %d", retval);

	return retval;
}