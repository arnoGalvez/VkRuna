// Copyright (c) 2021 Arno Galvez

#include "platform/Console.h"
#include "platform/Sys.h"
#include "platform/Window.h"

//#include "renderer/VFX.h"

#include <iostream>

using namespace vkRuna;
using namespace vkRuna::sys;

int CALLBACK wWinMain( _In_ HINSTANCE	  hInstance,
					   _In_opt_ HINSTANCE hPrevInstance,
					   _In_ LPWSTR		  lpCmdLine,
					   _In_ int			  nShowCmd )
{
	RedirectIOToConsole();

	try
	{
		// TODO wrap those Init calls in a global sys init
		// TODO redo window.Init
		winProps_t windowInputParameters {};
		windowInputParameters.width		= 900;
		windowInputParameters.height	= 600;
		windowInputParameters.name		= std::wstring( L"VkRuna - Alpha 0.1 (Win x_64) @ copyright 2021 Arno Galvez" );
		windowInputParameters.hInstance = hInstance;

		Window &window = Window::GetInstance();
		if ( !window.Init( windowInputParameters ) )
		{
			return 1;
		}

		while ( window.Frame() )
			;

		if ( window.GetExitCode() != exitCode_t::EC_SUCCESS )
		{
			return 2;
		}

		window.Shutdown();
	}
	catch ( const std::exception &e )
	{
		FatalError( "%s", e.what() );
	}

	return 0;
}