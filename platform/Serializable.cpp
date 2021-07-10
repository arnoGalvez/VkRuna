// Copyright (c) 2021 Arno Galvez

#include "platform/Serializable.h"

#include "platform/Sys.h"
#include "platform/defines.h"

#include <iostream>

namespace vkRuna
{
using namespace sys;

SerializableData::~SerializableData() {}

template< typename T >
void ClearType( int count, void *p )
{
	T *rp = reinterpret_cast< T * >( p );
	if ( count > 1 )
	{
		delete[] rp;
	}
	else
	{
		delete rp;
	}
}

// template<>
// void ClearType< SerializableData >( int count, void *p )
//{
//	SerializableData *rp = reinterpret_cast< SerializableData * >( p );
//	for ( int i = 0; i < count; i++ )
//	{
//		rp[ i ].Clear();
//		// std::cout << "Calling clear on Serializable child" << std::endl;
//	}
//
//	if ( count > 1 )
//	{
//		delete[] rp;
//	}
//	else
//	{
//		delete rp;
//	}
//}

void SerializableData::Clear()
{
	static_assert( SVT_COUNT == 4, "Unhandled new type serializable type" );

	if ( !value )
	{
		return;
	}

	switch ( type )
	{
		/*case SVT_OBJECT:
		{
			ClearType< SerializableData >( count, value );
			break;
		}*/
		case SVT_INT:
		{
			ClearType< int >( count, value );
			break;
		}
		case SVT_FLOAT:
		{
			ClearType< float >( count, value );
			break;
		}
		case SVT_STRING:
		{
			ClearType< std::string >( count, value );
			break;
		}
		case SVT_COLOR:
		{
			ClearType< float >( count, value );
			break;
		}
		default:
		{
			FatalError( "Unknown SerializableData type." );
			break;
		}
	}

	value = nullptr;
}

const char *SVTToString( serializableValue_t svt )
{
	static_assert( SVT_COUNT == 4, "Unhandled new type serializable type" );

	switch ( svt )
	{
		// SWITCH_CASE_STRING( SVT_OBJECT );
		SWITCH_CASE_STRING( SVT_INT );
		SWITCH_CASE_STRING( SVT_FLOAT );
		SWITCH_CASE_STRING( SVT_STRING );
		SWITCH_CASE_STRING( SVT_COLOR );
		SWITCH_CASE_STRING( SVT_COUNT );
		SWITCH_CASE_STRING( SVT_UNKNOWN );
	}

	return "???";
}

void PrintRec( const SerializableData &sd, int depth )
{
	static_assert( SVT_COUNT == 4, "Unhandled new type serializable type" );

	std::string tabs( "    ", depth );

	std::cout << tabs << "key: " << sd.key << '\n';
	std::cout << tabs << "type: " << SVTToString( sd.type ) << '\n';
	std::cout << tabs << "count: " << sd.count << '\n';

	if ( sd.type == SVT_UNKNOWN || sd.type == SVT_COUNT )
	{
		return;
	}

	std::cout << tabs << "value: ";
	for ( size_t i = 0; i < sd.count; i++ )
	{
		switch ( sd.type )
		{
			/*case SVT_OBJECT:
			{
				std::cout << "\n";
				SerializableData *obj = reinterpret_cast< SerializableData * >( sd.value );
				PrintRec( obj[ i ], depth + 1 );
				break;
			}*/
			case SVT_INT:
			{
				int *p = reinterpret_cast< int * >( sd.value );
				std::cout << p[ i ] << " ";
				break;
			}
			case SVT_FLOAT:
			{
				float *p = reinterpret_cast< float * >( sd.value );
				std::cout << p[ i ] << " ";
				break;
			}
			case SVT_STRING:
			{
				std::string *p = reinterpret_cast< std::string * >( sd.value );
				std::cout << p[ i ] << " ";
				break;
			}
			case SVT_COLOR:
			{
				float *p = reinterpret_cast< float * >( sd.value );
				std::cout << p[ i ] << " ";
				break;
			}
			default: break;
		}
	}

	std::cout << '\n';
}

void SerializableData::Print() const
{
	PrintRec( *this, 0 );
}

} // namespace vkRuna
