// Copyright (c) 2021 Arno Galvez

#pragma once

#include "platform/Sys.h" // #TODO get error handling out of this header

#include <string>

namespace vkRuna
{
enum serializableValue_t
{
	SVT_INT,
	SVT_FLOAT,
	SVT_STRING,
	SVT_COLOR,
	SVT_COUNT,
	SVT_UNKNOWN,

};

struct SerializableData
{
	serializableValue_t type = SVT_UNKNOWN;
	std::string			key {};
	int					count = 0;
	void *				value = nullptr;

	~SerializableData();
	void Clear();
	void Print() const;
};

class ISerializable
{
   public:
	virtual ~ISerializable() {}

	virtual void Load( const char *path ) = 0;
	virtual bool Save( const char *path ) = 0;
};

template< class Archive, typename T >
void SaveType( Archive &ar, const SerializableData &obj )
{
	T *				 p = reinterpret_cast< T * >( obj.value );
	std::vector< T > vec;
	vec.reserve( obj.count );
	for ( int i = 0; i < obj.count; i++ )
	{
		vec.emplace_back( p[ i ] );
	}
	ar( ::cereal::make_nvp( "value", vec ) );
}

template< class Archive >
void save( Archive &ar, const SerializableData &obj )
{
	ar( ::cereal::make_nvp( "type", obj.type ) );
	ar( ::cereal::make_nvp( "key", obj.key ) );
	ar( ::cereal::make_nvp( "count", obj.count ) );
	switch ( obj.type )
	{
		case vkRuna::SVT_INT:
		{
			SaveType< Archive, int >( ar, obj );
			break;
		}
		case vkRuna::SVT_FLOAT:
		case vkRuna::SVT_COLOR:
		{
			SaveType< Archive, float >( ar, obj );
			break;
		}
		case vkRuna::SVT_STRING:
		{
			SaveType< Archive, std::string >( ar, obj );
			break;
		}
		default:
		{
			FatalError( "When serializing object of type SerializableData: unknown type %d.", obj.type );
			break;
		}
	}
}

template< class Archive, typename T >
void LoadType( Archive &ar, SerializableData &obj )
{
	std::vector< T > vec;
	ar( ::cereal::make_nvp( "value", vec ) );

	T *p = obj.count > 1 ? new T[ obj.count ] : new T;
	for ( size_t i = 0; i < vec.size(); i++ )
	{
		p[ i ] = vec[ i ];
	}

	obj.count = static_cast< int >( vec.size() );
	obj.value = p;
}

template< class Archive >
void load( Archive &ar, SerializableData &obj )
{
	ar( ::cereal::make_nvp( "type", obj.type ) );
	ar( ::cereal::make_nvp( "key", obj.key ) );
	ar( ::cereal::make_nvp( "count", obj.count ) );
	switch ( obj.type )
	{
		case vkRuna::SVT_INT:
		{
			LoadType< Archive, int >( ar, obj );
			break;
		}
		case vkRuna::SVT_FLOAT:
		case vkRuna::SVT_COLOR:
		{
			LoadType< Archive, float >( ar, obj );
			break;
		}
		case vkRuna::SVT_STRING:
		{
			LoadType< Archive, std::string >( ar, obj );
			break;
		}
		default:
		{
			FatalError( "When serializing object of type SerializableData: unknown type %d.", obj.type );
			break;
		}
	}
}

} // namespace vkRuna
