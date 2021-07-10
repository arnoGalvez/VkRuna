// Copyright (c) 2021 Arno Galvez

#pragma once

#include "renderer/Shader.h"

#include <cstdarg>
#include <functional>
#include <string>

namespace vkRuna
{
enum event_t
{
	EV_BEFORE_SHADER_PARSING
};

class Event
{
   public:
	virtual ~Event() {}
	Event( event_t e )
		: m_type( e )
	{
	}

	event_t GetType() { return m_type; }
	bool	IsOfType( event_t type ) { return type == m_type; }

	NO_DISCARD virtual bool Call( int dummy... ) = 0;

   private:
	event_t m_type;
};

class EventOnShaderRead : public Event
{
   public:
	using Func = std::function< bool( std::string *, shaderStage_t ) >;

   public:
	EventOnShaderRead( Func &&f )
		: Event( EV_BEFORE_SHADER_PARSING )
		, m_f( f ) {};

	NO_DISCARD virtual bool Call( int dummy... )
	{
		va_list args;
		va_start( args, dummy );

		std::string * str	= va_arg( args, std::string * );
		shaderStage_t stage = va_arg( args, shaderStage_t );

		bool ret = Call( str, stage );

		va_end( args );

		return ret;
	}

	bool Call( std::string *s, shaderStage_t stage ) { return m_f( s, stage ); }

   private:
	Func m_f;
};

} // namespace vkRuna