// Copyright (c) 2021 Arno Galvez

#include "platform/Sys.h"

#include "platform/Check.h"
#include "platform/Window.h"
#include "platform/defines.h"

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <direct.h>
#include <windows.h>

namespace vkRuna
{
namespace sys
{
static const uint32_t MAX_QUED_EVENTS  = 256;
static const uint32_t MASK_QUED_EVENTS = MAX_QUED_EVENTS - 1;

static sysEvent_t eventQue[ MAX_QUED_EVENTS ]; // circular buffer
static int		  eventHead = 0;
static int		  eventTail = 0;

static char errorMessage[ 4096 ];

void Init()
{
	std::memset( eventQue, 0, sizeof( *eventQue ) * MASK_QUED_EVENTS );

	::setlocale( LC_CTYPE, ".UTF8" );

	std::string exeDir = ExtractDirPath( GetExePath() );
	if ( Chdir( exeDir.c_str() ) != sysCallRet_t::SUCCESS )
	{
		FatalError( "Could not change current working directory." );
	}
}

void Shutdown() {}

sysCallRet_t Chdir( const char *path )
{
	int ret = ::_chdir( path );
	if ( ret == -1 )
	{
		errno_t err = errno;
		switch ( err )
		{
			case ( ENOENT ): return sysCallRet_t::PATH_NOT_FOUND;
			case ( EINVAL ): return sysCallRet_t::NULL_PARAM;
			default: return sysCallRet_t::UNKNOWN;
		}
	}

	return sysCallRet_t::SUCCESS;
}

sysCallRet_t Mkdir( const char *path )
{
	int ret = ::_mkdir( path );
	if ( ret == -1 )
	{
		errno_t err = errno;
		switch ( err )
		{
			case ( EEXIST ): return sysCallRet_t::DIR_EXIST;
			case ( ENOENT ): return sysCallRet_t::PATH_NOT_FOUND;
			default: return sysCallRet_t::UNKNOWN;
		}
	}

	return sysCallRet_t::SUCCESS;
}

std::string GetExePath()
{
	// https://stackoverflow.com/questions/1528298/get-path-of-executable
	typedef std::vector< char >			   char_vector;
	typedef std::vector< char >::size_type size_type;
	char_vector							   buf( 1024, 0 );
	size_type							   size			  = buf.size();
	bool								   havePath		  = false;
	bool								   shouldContinue = true;
	do
	{
		DWORD result	= GetModuleFileNameA( nullptr, &buf[ 0 ], static_cast< DWORD >( size ) );
		DWORD lastError = GetLastError();
		if ( result == 0 )
		{
			shouldContinue = false;
		}
		else if ( result < size )
		{
			havePath	   = true;
			shouldContinue = false;
		}
		else if ( result == size && ( lastError == ERROR_INSUFFICIENT_BUFFER || lastError == ERROR_SUCCESS ) )
		{
			size *= 2;
			buf.resize( size );
		}
		else
		{
			shouldContinue = false;
		}
	} while ( shouldContinue );

	std::string ret = &buf[ 0 ];
	return ret;
}

std::string ExtractDirPath( const std::string &path )
{
	std::string::size_type n = path.rfind( '\\' );
	if ( n != std::string::npos )
	{
		return path.substr( 0, n );
	}

	Error( "Could not extract directory from path \"%s\"", path.c_str() );

	return path;
}

std::string ExtractFileName( const std::string &path )
{
	std::string::size_type n = path.rfind( '\\' );
	if ( n != std::string::npos )
	{
		return path.substr( n + 1 );
	}

	Error( "Could not extract file name from path \"%s\"", path.c_str() );

	return path;
}

void Log( const char *fmt... )
{
	std::printf( "\x1b[94mLog\x1b[0m: " );
	va_list argptr;

	va_start( argptr, fmt );
	std::vprintf( fmt, argptr );
	va_end( argptr );

	std::printf( "\n" );
}

void Error( const char *fmt... )
{
	va_list argptr;

	va_start( argptr, fmt );
	std::vsnprintf( errorMessage, sizeof( errorMessage ), fmt, argptr );
	va_end( argptr );

	std::printf( "\x1b[33mERROR\x1b[0m: %s\n", errorMessage );
}

void FatalError( const char *fmt... )
{
	DEBUG_BREAK();

	va_list argptr;

	va_start( argptr, fmt );
	Error( fmt );
	va_end( argptr );

	Window::GetInstance().OnFatalError();

	std::printf( "\x1b[31mFATAL ERROR\x1b[0m: close window to quit..." );

	while ( 1 )
	{
		Sys_ClearEvents();

		if ( Sys_GenerateEvents() )
		{
			break;
		}
		/*for ( sysEvent_t ev = Sys_GetEvent(); ev.evType != SE_NONE; ev = Sys_GetEvent() ) {
			if ( ev.IsKeyEvent() ) {
				break;
			}
		}*/
	}

	::ExitProcess( 0 );
}

int Sys_GenerateEvents()
{
	MSG msg;
	while ( ::PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
		switch ( msg.message )
		{
			case WM_CLOSE: return 1;
			default: break;
		}

		::TranslateMessage( &msg );
		::DispatchMessageW( &msg );
	}

	return 0;
}

void Sys_QueEvent( sysEventType_t type, int value, int value2, int value3 )
{
	sysEvent_t &ev = eventQue[ eventHead & MASK_QUED_EVENTS ];

	if ( eventHead - eventTail >= MAX_QUED_EVENTS )
	{
		Error( "Sys_QueEvent overflow\n" );
		++eventTail;
	}

	++eventHead;

	ev.evType	= type;
	ev.evValue	= value;
	ev.evValue2 = value2;
	ev.evValue3 = value3;
}

sysEvent_t Sys_GetEvent()
{
	sysEvent_t ev;

	// return if we have data
	if ( eventHead > eventTail )
	{
		return eventQue[ ( eventTail++ ) & MASK_QUED_EVENTS ];
	}

	// return the empty event
	memset( &ev, 0, sizeof( ev ) );

	return ev;
}

std::string ReadFile( const char *path )
{
	std::ifstream file( path, std::ios::ate | std::ios::in );
	if ( !file.is_open() )
	{
		throw std::ios::failure( "Failed to load file \"" + std::string( path ) + "\"." );
	}

	auto		size = file.tellg();
	std::string buff( size, ' ' );
	file.seekg( 0, std::ios::beg );
	file.read( &buff[ 0 ], size );

	return buff;
}

void Sys_ClearEvents()
{
	eventHead = eventTail = 0;
}

const char *KeyToString( keyNum_t key )
{
	switch ( key )
	{
		SWITCH_CASE_STRING( K_LMOUSE );
		SWITCH_CASE_STRING( K_RMOUSE );
		SWITCH_CASE_STRING( K_ESC );
		SWITCH_CASE_STRING( K_SPACE );
		SWITCH_CASE_STRING( K_LEFT );
		SWITCH_CASE_STRING( K_UP );
		SWITCH_CASE_STRING( K_RIGHT );
		SWITCH_CASE_STRING( K_DOWN );
		SWITCH_CASE_STRING( K_0 );
		SWITCH_CASE_STRING( K_1 );
		SWITCH_CASE_STRING( K_2 );
		SWITCH_CASE_STRING( K_3 );
		SWITCH_CASE_STRING( K_4 );
		SWITCH_CASE_STRING( K_5 );
		SWITCH_CASE_STRING( K_6 );
		SWITCH_CASE_STRING( K_7 );
		SWITCH_CASE_STRING( K_8 );
		SWITCH_CASE_STRING( K_9 );
		SWITCH_CASE_STRING( K_A );
		SWITCH_CASE_STRING( K_B );
		SWITCH_CASE_STRING( K_C );
		SWITCH_CASE_STRING( K_D );
		SWITCH_CASE_STRING( K_E );
		SWITCH_CASE_STRING( K_F );
		SWITCH_CASE_STRING( K_G );
		SWITCH_CASE_STRING( K_H );
		SWITCH_CASE_STRING( K_I );
		SWITCH_CASE_STRING( K_J );
		SWITCH_CASE_STRING( K_K );
		SWITCH_CASE_STRING( K_L );
		SWITCH_CASE_STRING( K_M );
		SWITCH_CASE_STRING( K_N );
		SWITCH_CASE_STRING( K_O );
		SWITCH_CASE_STRING( K_P );
		SWITCH_CASE_STRING( K_Q );
		SWITCH_CASE_STRING( K_R );
		SWITCH_CASE_STRING( K_S );
		SWITCH_CASE_STRING( K_T );
		SWITCH_CASE_STRING( K_U );
		SWITCH_CASE_STRING( K_V );
		SWITCH_CASE_STRING( K_W );
		SWITCH_CASE_STRING( K_X );
		SWITCH_CASE_STRING( K_Y );
		SWITCH_CASE_STRING( K_Z );
		SWITCH_CASE_STRING( K_COUNT );
		default: return "?";
	}
}

bool ExecuteAndWait( char *cmdLine )
{
	STARTUPINFO			si;
	PROCESS_INFORMATION pi;
	DWORD				ex = 0;

	::ZeroMemory( &si, sizeof( si ) );
	si.cb = sizeof( si );
	::ZeroMemory( &pi, sizeof( pi ) );

	// Start the child process.
	if ( !::CreateProcess( NULL,	// No module name (use command line)
						   cmdLine, // Command line
						   NULL,	// Process handle not inheritable
						   NULL,	// Thread handle not inheritable
						   FALSE,	// Set handle inheritance to FALSE
						   0,		// No creation flags
						   NULL,	// Use parent's environment block
						   NULL,	// Use parent's starting directory
						   &si,		// Pointer to STARTUPINFO structure
						   &pi )	// Pointer to PROCESS_INFORMATION structure
	)
	{
		Error( "CreateProcess failed (%d).\n", ::GetLastError() );
		return false;
	}

	// Wait until child process exits.
	::WaitForSingleObject( pi.hProcess, INFINITE );

	if ( !::GetExitCodeProcess( pi.hProcess, &ex ) )
	{
		Error( "GetExitCodeProcess failed (%d).", ::GetLastError() );
		return false;
	}

	if ( ex != 0 )
	{
		Error( "The following command failed:\n%s", cmdLine );
		return false;
	}

	// Close process and thread handles.
	::CloseHandle( pi.hProcess );
	::CloseHandle( pi.hThread );

	return true;
}

int64_t ClockTicksFrequency()
{
	static LARGE_INTEGER Frequency { 0 };

	if ( Frequency.QuadPart == 0 )
	{
		QueryPerformanceFrequency( &Frequency );
	}

	return Frequency.QuadPart;
}

int64_t GetClockTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter( &ticks );

	return ticks.QuadPart;
}

} // namespace sys
} // namespace vkRuna
