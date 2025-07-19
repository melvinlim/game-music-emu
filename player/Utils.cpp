#include "Utils.h"

//	https://stackoverflow.com/questions/2347770/how-do-you-clear-the-console-screen-in-c
void ClearScreen()
{
	if (!cur_term)
	{
		int result;
		setupterm( NULL, STDOUT_FILENO, &result );
		if (result <= 0) return;
	}

	putp( tigetstr( "clear" ) );
}
