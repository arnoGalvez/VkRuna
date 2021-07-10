// Copyright (c) 2021 Arno Galvez

#include "game/GameLocal.h"

#include "external/glm/gtx/string_cast.hpp"
#include "platform/Window.h"
#include "renderer/RenderSystem.h"
#include "renderer/ShaderLexer.h"
#include "ui/Ui.h"

#include <cmath>
#include <cstdio>
#include <iostream>

namespace vkRuna
{
using namespace sys;

GameLocal	g_gameLocal;
Game *const g_game = &g_gameLocal;

const double camSpeed	 = 10.0;
const double camRotSpeed = 6000.0;

void GameLocal::Init()
{
	std::memset( m_gamekeys, 0, sizeof( m_gamekeys ) );
	std::memset( m_actionstates, 0, sizeof( m_actionstates ) );
	std::memset( m_mousePos, 0, sizeof( m_mousePos ) );

	BindAction( UA_MOVERIGHT, K_RIGHT );
	BindAction( UA_MOVELEFT, K_LEFT );
	BindAction( UA_MOVEFORWARD, K_UP );
	BindAction( UA_MOVEBACK, K_DOWN );
	BindAction( UA_CAMMOVE, K_RMOUSE );

	// #TODO silly behavior at the moment
	/*BindAction( UA_PAUSE, K_P );
	BindAction( UA_HIDE_UI, K_U );*/

	// BindAction( UA_QUIT, K_ESC );

	InitCursor();

	cameraFrame_t &camFrame = m_cmd.cam.GetFrame();
	camFrame.p				= { -3.0f, 0.0f, 0.0f };
	camFrame.a				= 0.0f;
	camFrame.fov			= 90.0f;

	g_uiManager.Init();
}

void GameLocal::Shutdown()
{
	g_uiManager.Shutdown();
}

void GameLocal::RunFrame()
{
	ProcessEvents();

	BuildCmd();

	G_Ticker();

	EndFrame();
}

int GameLocal::GetCamProjView( const float **ptr )
{
	*ptr = m_cmd.cam.GetProjPtr();
	return 2 * 4 * 4;
}

void GameLocal::InitCursor()
{
	Window &win = Window::GetInstance();
	win.SetCursorPosCenter();
	win.HideCursor();
}

void GameLocal::ProcessEvents()
{
	if ( g_uiManager.IsAnyItemActive() )
	{
		return;
	}

	for ( sysEvent_t ev = Sys_GetEvent(); ev.evType != SE_NONE; ev = Sys_GetEvent() )
	{
		if ( M_Responder( ev ) )
		{
			continue;
		}

		G_Responder( ev );
	}
}

bool GameLocal::M_Responder( sysEvent_t ev )
{
	if ( ev.evType == SE_KEY )
	{
		keyNum_t	 key	= ev.GetKey();
		userAction_t action = GetKeyAction( key );
		if ( action == UA_QUIT )
		{
			Window::GetInstance().PostQuitMessage();
			return true;
		}
	}

	return false;
}

bool GameLocal::G_Responder( sysEvent_t ev )
{
	Window &win = Window::GetInstance();
	// in game logic
	switch ( ev.evType )
	{
		case SE_KEY:
		{
			keyNum_t key = ev.GetKey();
			if ( key < K_COUNT )
			{
				// std::cout << KeyToString( key ) << ( ev.evValue2 ? ": down" : ": up" ) << '\n';
				m_gamekeys[ key ].down	 = ev.evValue2;
				userAction_t action		 = GetKeyAction( key );
				m_actionstates[ action ] = IsKeyDown( key );
			}

			break;
		}

		case SE_MOUSE_ABSOLUTE:
		{
			m_mousePos[ 0 ] = ev.GetXCoord();
			m_mousePos[ 1 ] = ev.GetYCoord();
			break;
		}

		default: break;
	}

	return true; // ev always consumed
}

void GameLocal::BuildCmd()
{
	MouseMove();
	KeyMove();
}

void GameLocal::MouseMove()
{
	// Log( "mouse pos:(%d, %d)", m_mousePos[ 0 ], m_mousePos[ 1 ] );

	if ( ActionState( UA_CAMMOVE ) )
	{
		Window &		  win	   = Window::GetInstance();
		const winProps_t &winprops = win.GetProps();

		if ( m_firstCamMoveFrame )
		{
			m_mousePos[ 0 ]		= (int)winprops.width / 2;
			m_mousePos[ 1 ]		= (int)winprops.height / 2;
			m_firstCamMoveFrame = false;
		}

		double dt		= win.GetFrameDeltaTime();
		int	   winWidth = 0, winHeight = 0;
		win.GetScreenDim( winWidth, winHeight );
		double da = double( m_mousePos[ 0 ] - ( (int)winprops.width / 2 ) ) / double( winWidth );
		double di = double( m_mousePos[ 1 ] - ( (int)winprops.height / 2 ) ) / double( winHeight );

		double rotD = camRotSpeed * dt;

		cameraFrame_t &camFrame = m_cmd.cam.GetFrame();
		camFrame.a += static_cast< float >( da * rotD );
		camFrame.i += static_cast< float >( di * rotD );

		camFrame.i = glm::clamp( camFrame.i, 1.0f, 179.0f );
		camFrame.a = std::fmod( camFrame.a, 360.0f );

		// Log( "camFrame.i: %f", camFrame.i );

		win.SetCursorPosCenter();
		win.HideCursor();
	}
	else
	{
		m_firstCamMoveFrame = true;
	}
}

// Update game state according to keyboard input
void GameLocal::KeyMove()
{
	// throw std::logic_error( "The method or operation is not implemented." );
	double dt = Window::GetInstance().GetFrameDeltaTime();
	float  d  = static_cast< float >( camSpeed * dt );

	float side	  = 0;
	float forward = 0;

	side += d * ActionState( UA_MOVERIGHT );
	side -= d * ActionState( UA_MOVELEFT );

	forward += d * ActionState( UA_MOVEFORWARD );
	forward -= d * ActionState( UA_MOVEBACK );

	cameraFrame_t &camFrame = m_cmd.cam.GetFrame();
	camFrame.p += side * m_cmd.cam.GetRight();
	camFrame.p += forward * m_cmd.cam.GetFront();

	// TO_COUT_GLM( camFrame.p );

	if ( ActionState( UA_PAUSE ) )
	{
		m_paused = !m_paused;
	}

	if ( ActionState( UA_HIDE_UI ) )
	{
		m_showUI = !m_showUI;
	}
}

// Send cam update
void GameLocal::G_Ticker()
{
	P_Ticker();
	g_uiManager.Ticker( m_showUI );
}

void GameLocal::P_Ticker()
{
	m_cmd.cam.UpdateProjView();

	if ( !m_paused )
	{
		m_deltaFrame = Window::GetInstance().GetFrameDeltaTime();
		m_time += m_deltaFrame;
	}
	else
	{
		m_deltaFrame = 0.0;
	}

	// #TODO this part should be handle by the render system.
	float		fDeltaFrame	 = static_cast< float >( m_deltaFrame );
	float		fTime		 = static_cast< float >( m_time );
	float		timeVec[ 8 ] = { fDeltaFrame, fDeltaFrame, fDeltaFrame, fDeltaFrame, fTime, fTime, fTime, fTime };
	const char *varNames[]	 = { GlobalsTokenizer::GetProjStr(),
								 GlobalsTokenizer::GetViewStr(),
								 GlobalsTokenizer::GetDeltaFrameStr(),
								 GlobalsTokenizer::GetTimeStr() };

	render::g_renderSystem->SetUBOVar( 2, varNames, m_cmd.cam.GetProjPtr() );
	render::g_renderSystem->SetUBOVar( 2, varNames + 2, timeVec );
}

void GameLocal::EndFrame() {}

} // namespace vkRuna
