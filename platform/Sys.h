// Copyright (c) 2021 Arno Galvez

#pragma once

#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace vkRuna
{
namespace sys
{
// https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
enum keyNum_t
{
	K_LMOUSE = 0x01,
	K_RMOUSE,

	K_ESC	= 0x1B,
	K_SPACE = 0x20,

	K_LEFT = 0x25,
	K_UP,
	K_RIGHT,
	K_DOWN,

	K_0 = 0x30,
	K_1,
	K_2,
	K_3,
	K_4,
	K_5,
	K_6,
	K_7,
	K_8,
	K_9,

	K_A = 0x41,
	K_B,
	K_C,
	K_D,
	K_E,
	K_F,
	K_G,
	K_H,
	K_I,
	K_J,
	K_K,
	K_L,
	K_M,
	K_N,
	K_O,
	K_P,
	K_Q,
	K_R,
	K_S,
	K_T,
	K_U,
	K_V,
	K_W,
	K_X,
	K_Y,
	K_Z,

	K_COUNT
};

enum sysEventType_t
{
	SE_NONE,
	SE_KEY,			   // evValue is a key code, evValue2 is the down flag, evValues3 is rep count
	SE_MOUSE_ABSOLUTE, // evValue and evValue2 are absolute coordinates in the window's client area.
};

struct sysEvent_t
{
	sysEventType_t evType	= SE_NONE;
	int			   evValue	= 0;
	int			   evValue2 = 0;
	int			   evValue3 = 0;

	bool	 IsKeyEvent() const { return evType == SE_KEY; }
	bool	 IsMouseEvent() const { return evType == SE_MOUSE_ABSOLUTE; }
	bool	 IsKeyDown() const { return evValue2 != 0; }
	keyNum_t GetKey() const { return static_cast< keyNum_t >( evValue ); }
	int		 GetXCoord() const { return evValue; }
	int		 GetYCoord() const { return evValue2; }
};

enum class sysCallRet_t
{
	SUCCESS,
	DIR_EXIST,
	PATH_NOT_FOUND,
	NULL_PARAM,
	UNKNOWN,
};

void Init();
void Shutdown();

sysCallRet_t Chdir( const char *path );
sysCallRet_t Mkdir( const char *path );
std::string	 GetExePath();
std::string	 ExtractDirPath( const std::string &path );
std::string	 ExtractFileName( const std::string &path );

void Log( const char *fmt... );
void Error( const char *fmt... );
void FatalError( const char *fmt... );

int		   Sys_GenerateEvents();
void	   Sys_QueEvent( sysEventType_t type, int value, int value2, int value3 );
sysEvent_t Sys_GetEvent();
void	   Sys_ClearEvents();

const char *KeyToString( keyNum_t key );

bool ExecuteAndWait( char *cmdLine );

int64_t ClockTicksFrequency();
int64_t GetClockTicks();

template< typename CharT >
std::vector< CharT > ReadBinary( const char *relativePath );
std::string			 ReadFile( const char *path );

#include "Sys.inl"

} // namespace sys
} // namespace vkRuna
