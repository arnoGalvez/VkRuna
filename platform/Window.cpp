// Copyright (c) 2021 Arno Galvez

#include "platform/Window.h"

#include "game/GameLocal.h"
#include "platform/Sys.h"
#include "renderer/Backend.h"
#include "renderer/RenderSystem.h"
#include "renderer/VkRenderCommon.h"

#include <algorithm>
#include <iostream>

namespace vkRuna
{
extern LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

Window::Window()
	: m_props()
	, m_hWnd( 0 )
{
}

static Window g_win;

Window &Window::GetInstance()
{
	return g_win;
}

Window::~Window()
{
	if ( m_hWnd )
	{
		DestroyWindow( m_hWnd );
	}

	if ( m_props.hInstance )
	{
		UnregisterClassW( m_props.name.c_str(), m_props.hInstance );
	}
}

bool Window::Init( const winProps_t &windowInputParameters )
{

	sys::Init();
	InitTimeCounters();

	this->m_props = windowInputParameters;

	std::wstring windowClassName( windowInputParameters.name + L"_window_class" );
	auto		 hInstance = ::GetModuleHandle( nullptr );

	if ( !RegisterWindowClass( windowClassName, hInstance ) )
	{
		return false;
	}

	RECT winRect = { 0, 0, static_cast< LONG >( m_props.width ), static_cast< LONG >( m_props.height ) };
	::AdjustWindowRect( &winRect, WS_OVERLAPPEDWINDOW, FALSE );

	auto screenWidth  = ::GetSystemMetrics( SM_CXSCREEN );
	auto screenHeight = ::GetSystemMetrics( SM_CYSCREEN );

	screenDim[ 0 ] = screenWidth;
	screenDim[ 1 ] = screenHeight;

	auto winWidth  = winRect.right - winRect.left;
	auto winHeight = winRect.bottom - winRect.top;
	auto winX	   = std::max< LONG >( 0, ( screenWidth - winWidth ) / 2 );
	auto winY	   = std::max< LONG >( 0, ( screenHeight - winHeight ) / 2 );

	m_hWnd = ::CreateWindowExW( NULL,
								windowClassName.c_str(),
								m_props.name.c_str(),
								WS_OVERLAPPEDWINDOW,
								winX,
								winY,
								winWidth,
								winHeight,
								NULL,
								NULL,
								hInstance,
								this ); // ??? this ???
	if ( m_hWnd == NULL )
		return false;

	//::SetWindowTextW( m_hWnd, m_inputParameters.name.c_str() );

	render::Backend &	  renderBackend = render::Backend::GetInstance();
	render::RenderSystem &renderSystem	= render::RenderSystem::GetInstance();
	renderBackend.Init();
	renderSystem.Init();

	g_game->Init();

	ShowWindow( m_hWnd, SW_SHOWMAXIMIZED );
	//	UpdateWindow( m_hWnd );

	QueryWindowClient();

	return true;
}

void Window::Shutdown()
{
	render::RenderSystem &renderSystem	= render::RenderSystem::GetInstance();
	render::Backend &	  renderBackend = render::Backend::GetInstance();

	renderSystem.Shutdown();
	renderBackend.Shutdown();

	g_game->Shutdown();

	KillWindow();

	sys::Shutdown();
}

bool Window::Frame()
{
	UpdateTimeCounters();

	if ( sys::Sys_GenerateEvents() )
	{
		m_exitCode = exitCode_t::EC_SUCCESS;
		return false;
	}

	// Game logic
	{
		g_game->RunFrame();
	}

	// Rendering
	{
		static render::Backend &	 renderBackend = render::Backend::GetInstance();
		static render::RenderSystem &renderSystem  = render::RenderSystem::GetInstance();

		render::gpuCmd_t *preRenderCmds		 = nullptr;
		render::gpuCmd_t *renderCmds		 = nullptr;
		int				  preRenderCmdsCount = 0;
		int				  renderCmdsCount	 = 0;

		renderSystem.BeginFrame();
		renderSystem.EndFrame();

		// #TODO this part could go in renderSystem.EndFrame() ???
		preRenderCmdsCount = renderSystem.GetPreRenderCmds( &preRenderCmds );
		renderCmdsCount	   = renderSystem.GetRenderCmds( &renderCmds );

		renderBackend.ExecuteCommands( preRenderCmdsCount, preRenderCmds, renderCmdsCount, renderCmds );
		renderBackend.Present();
	}

	sys::Sys_ClearEvents();
	return true;
}

void Window::PostQuitMessage()
{
	PostMessage( m_hWnd, WM_CLOSE, 0, 0 );
}

void Window::SetCursorPosCli( int x, int y )
{
	::SetCursorPos( x + m_props.x, y + m_props.y );
}

void Window::SetCursorPosCenter()
{
	SetCursorPosCli( m_props.width / 2, m_props.height / 2 );
}

void Window::GetScreenDim( int &width, int &height )
{
	width  = screenDim[ 0 ];
	height = screenDim[ 1 ];
}

void Window::GetCliRectCenter( int &x, int &y )
{
	x = ( m_props.width / 2 ) + m_props.x;
	y = ( m_props.height / 2 ) + m_props.y;
}

void Window::HideCursor()
{
	::SetCursor( NULL );
}

void Window::ShowCursor()
{
	::SetCursor( LoadCursor( NULL, IDC_ARROW ) );
}

void Window::OnFatalError()
{
	render::RenderSystem &renderSystem = render::RenderSystem::GetInstance();

	renderSystem.Shutdown();

	KillWindow();
}

bool Window::RegisterWindowClass( const std::wstring &windowClassName, HINSTANCE hInstance )
{
	WNDCLASSEXW wcexw {};

	wcexw.cbSize		= sizeof( WNDCLASSEX );
	wcexw.style			= CS_HREDRAW | CS_VREDRAW;
	wcexw.lpfnWndProc	= &WndProc;
	wcexw.cbClsExtra	= 0;
	wcexw.cbWndExtra	= 0;
	wcexw.hInstance		= hInstance;
	wcexw.hIcon			= ::LoadIcon( hInstance, NULL );
	wcexw.hCursor		= ::LoadCursor( NULL, IDC_ARROW );
	wcexw.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
	wcexw.lpszMenuName	= NULL;
	wcexw.lpszClassName = windowClassName.c_str();
	wcexw.hIconSm		= ::LoadIcon( hInstance, NULL );

	return ::RegisterClassExW( &wcexw );
}

void Window::QueryWindowClient()
{
	RECT rect;
	if ( ::GetClientRect( GetHWND(), &rect ) )
	{
		m_props.width  = rect.right - rect.left;
		m_props.height = rect.bottom - rect.top;
	}

	POINT clientRectScreenPos;
	clientRectScreenPos.x = 0;
	clientRectScreenPos.y = 0;
	if ( ::ClientToScreen( GetHWND(), &clientRectScreenPos ) )
	{
		m_props.x = clientRectScreenPos.x;
		m_props.y = clientRectScreenPos.y;
	}
}

void Window::InitTimeCounters()
{
	m_elapsedTime	= double( sys::GetClockTicks() ) / double( sys::ClockTicksFrequency() );
	m_lastFrameTime = m_elapsedTime;
}

void Window::UpdateTimeCounters()
{
	m_lastFrameTime = m_elapsedTime;
	m_elapsedTime	= double( sys::GetClockTicks() ) / double( sys::ClockTicksFrequency() );

	m_frameDeltaTime = m_elapsedTime - m_lastFrameTime;
}

void Window::KillWindow()
{
	if ( m_hWnd )
	{
		::ShowWindow( m_hWnd, NULL );
		::CloseWindow( m_hWnd );
		::DestroyWindow( m_hWnd );
		m_hWnd = nullptr;
	}
}

} // namespace vkRuna
