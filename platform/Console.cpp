// Copyright (c) 2021 Arno Galvez

#include "platform/Console.h"

#include <fcntl.h>
#include <fstream>
#include <io.h>
#include <iostream>
#include <stdio.h>
#include <windows.h>
#ifndef _USE_OLD_IOSTREAMS
using namespace std;
#endif

namespace vkRuna
{
// maximum mumber of lines the output console should have
static const WORD MAX_CONSOLE_LINES = 500;

void RedirectIOToConsole()
{
	// https://stackoverflow.com/a/46050762

	// Create a console for this application
	AllocConsole();

	// Get STDOUT handle
	HANDLE ConsoleOutput = GetStdHandle( STD_OUTPUT_HANDLE );
	int	   SystemOutput	 = _open_osfhandle( intptr_t( ConsoleOutput ), _O_TEXT );
	FILE * COutputHandle = _fdopen( SystemOutput, "w" );

	// Get STDERR handle
	HANDLE ConsoleError = GetStdHandle( STD_ERROR_HANDLE );
	int	   SystemError	= _open_osfhandle( intptr_t( ConsoleError ), _O_TEXT );
	FILE * CErrorHandle = _fdopen( SystemError, "w" );

	// Get STDIN handle
	HANDLE ConsoleInput = GetStdHandle( STD_INPUT_HANDLE );
	int	   SystemInput	= _open_osfhandle( intptr_t( ConsoleInput ), _O_TEXT );
	FILE * CInputHandle = _fdopen( SystemInput, "r" );

	DWORD dwMode = 0;
	GetConsoleMode( ConsoleOutput, &dwMode );
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode( ConsoleOutput, dwMode );

	// make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog point to console as well
	ios::sync_with_stdio( true );

	// Redirect the CRT standard input, output, and error handles to the console
	freopen_s( &CInputHandle, "CONIN$", "r", stdin );
	freopen_s( &COutputHandle, "CONOUT$", "w", stdout );
	freopen_s( &CErrorHandle, "CONOUT$", "w", stderr );

	// Clear the error state for each of the C++ standard stream objects. We need to do this, as
	// attempts to access the standard streams before they refer to a valid target will cause the
	// iostream objects to enter an error state. In versions of Visual Studio after 2005, this seems
	// to always occur during startup regardless of whether anything has been read from or written to
	// the console or not.
	std::wcout.clear();
	std::cout.clear();
	std::wcerr.clear();
	std::cerr.clear();
	std::wcin.clear();
	std::cin.clear();
}

} // namespace vkRuna

// End of File