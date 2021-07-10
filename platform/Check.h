#pragma once

#include <stdexcept>

#define STRINGIFY_MACRO( x ) STRINGIFY( x )
#define STRINGIFY( x )		 #x

#define CHECK_PRED( pred )                                                                                         \
	{                                                                                                              \
		bool predValue = pred;                                                                                     \
		if ( !predValue )                                                                                          \
		{                                                                                                          \
			throw std::runtime_error( __FILE__ " ( " STRINGIFY_MACRO( __LINE__ ) " ) failed predicate:\n" #pred ); \
		}                                                                                                          \
	}

#define CHECK_PRED_MSG( pred, msg )                                                             \
	{                                                                                           \
		bool predValue = pred;                                                                  \
		if ( !predValue )                                                                       \
		{                                                                                       \
			throw std::runtime_error( __FILE__ " ( " STRINGIFY_MACRO( __LINE__ ) " ) : " msg ); \
		}                                                                                       \
	}