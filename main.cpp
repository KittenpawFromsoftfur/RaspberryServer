#include "main.h"
#include "mainlogic.h"

/* TODO
	GENERAL
		main() with args so Port/settings can be set from command line etc...
		Unused variables
		config file?
		set/mosset --> set mosfet/gpio ; dasselbe mit allen kommandos wie clear...
		What happens to CPU if we launch per autostart?
		Test every single function again
		Clean up everything / Sort functions properly / Code formatter { 0 }
		core::isnumber()
		preferences per account like uppercase...

	BUGS
		writekey: when less lines than needed, it will take the last line instead of throwing error or making the necessary amount of lines.

	GPIO / MOSFET
		read single or multiple
		set single or multiple

	INTERFACE
		Make it so you can connect with a browser and click on buttons instead of sending commands

	END
		Documentation please!
*/

int main(int argc, char *argv[])
{
	for (int i = 0; i < argc; ++i)
	{
		printf("\nmain[%d] = <%s>", i, argv[i]);
	}

	printf("\n\n\n");

	return 0;
	CMainlogic mainlogic(LOG_FOLDERPATH, LOG_NAME);

	return mainlogic.EntryPoint();
}