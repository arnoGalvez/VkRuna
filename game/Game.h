// Copyright (c) 2021 Arno Galvez

#pragma once

#include <cstdint>

namespace vkRuna
{
enum userAction_t : uint8_t
{
	UA_NONE,

	UA_MOVERIGHT,
	UA_MOVELEFT,
	UA_MOVEFORWARD,
	UA_MOVEBACK,
	UA_CAMMOVE,

	UA_PAUSE,
	UA_HIDE_UI,

	UA_QUIT,

	UA_COUNT
};

struct key_t
{
	bool		 down;
	userAction_t action;
};

class Game
{
   public:
	virtual ~Game() = default;

	virtual void Init()		= 0;
	virtual void Shutdown() = 0;

	virtual void RunFrame() = 0;

	virtual void   BindAction( userAction_t action, int key ) = 0;
	virtual double GetDeltaFrame() const					  = 0;
	virtual int	   GetCamProjView( const float **ptr )		  = 0;
};

extern Game *const g_game;

} // namespace vkRuna
