#include "main.h"
#include "mainlogic.h"

/* TODO
	GENERAL
		Server has many functions with pointers to own members --> pass only slotindex
		CServer split structs into classes with their own functions --> First see which members can be put into class, like thrUpdate
		Clean up everything / Sort functions properly / Code formatter { 0 }
		set/mosset --> set mosfet/gpio ; dasselbe mit allen kommandos wie clear...
		main() with args so Port/settings can be set from command line etc...
		Unused variables
		config file?
		Test every single function again
		What happens to CPU if we launch per autostart?

	GPIO / MOSFET
		read single or multiple
		multi set

	INTERFACE
		Make it so you can connect with a browser and click on buttons instead of sending commands

	END
		Documentation please!
*/

int main()
{
	CMainlogic mainlogic(LOG_FOLDERPATH, LOG_NAME);

	return mainlogic.EntryPoint();
}