// Copyright (c) 2021 Arno Galvez

#pragma once

#include "game/Game.h"
#include "platform/Sys.h"
#include "rnLib/Camera.h"

namespace vkRuna
{
struct usercmd_t
{
	Camera cam;
};

class GameLocal : public Game
{
   public:
	void Init() final;
	void Shutdown() final;

	void RunFrame() final;

	inline void BindAction( userAction_t action, int key ) final;
	double		GetDeltaFrame() const final { return m_deltaFrame; };
	int			GetCamProjView( const float **ptr ) final;

   private:
	void InitCursor();

	void ProcessEvents();
	bool M_Responder( sys::sysEvent_t ev );
	bool G_Responder( sys::sysEvent_t ev );

	void BuildCmd();

	void MouseMove();
	void KeyMove();

	void G_Ticker();
	void P_Ticker();

	void EndFrame();

	bool		 IsKeyDown( sys::keyNum_t key ) const { return m_gamekeys[ key ].down; }
	userAction_t GetKeyAction( sys::keyNum_t key ) const { return m_gamekeys[ key ].action; }
	int			 ActionState( userAction_t action ) const { return m_actionstates[ action ]; }

   private:
	key_t  m_gamekeys[ sys::K_COUNT ] {};
	int	   m_actionstates[ UA_COUNT ] {};
	int	   m_mousePos[ 2 ] {};
	bool   m_firstCamMoveFrame = true;
	bool   m_paused			   = false;
	bool   m_showUI			   = true;
	double m_deltaFrame		   = 0.0;
	double m_time			   = 0.0;

	usercmd_t m_cmd;
};

extern GameLocal g_gameLocal;

void GameLocal::BindAction( userAction_t action, int key )
{
	m_gamekeys[ key ].action = action;
}

} // namespace vkRuna
