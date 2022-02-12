// Copyright (c) 2021 Arno Galvez

#pragma once

#define NO_DISCARD	  [[nodiscard]]
#define DEBUG_BREAK() __debugbreak()

#define SWITCH_CASE_STRING( x ) \
	case x:                     \
		return #x

#define TO_COUT( x )	 std::cout << #x ": " << ( x ) << '\n'
#define TO_COUT_GLM( x ) std::cout << #x ": " << glm::to_string( x ) << '\n'

// WARNING: everything after this macro is declared public
#define DEFAULT_CONSTRUCT_ASSIGN( className ) \
   public:                                    \
	className()					   = default; \
	className( const className & ) = default; \
	~className()				   = default; \
	className &operator=( const className & ) = default;

// WARNING: everything after this macro is declared public
#define NO_COPY_NO_ASSIGN( className )       \
   public:                                   \
	className( const className & ) = delete; \
	className( className && )	   = delete; \
	className &operator=( const className & ) = delete;

// WARNING: everything after this macro is declared public
#define DEFAULT_RVAL_CONSTRUCT_ASSIGN( className ) \
   public:                                         \
	className( className && ) = default;           \
	className &operator=( className && ) = default;

#define BASE_CLASS( baseClass ) using Base = baseClass;

template< typename T, size_t N >
char ( &ArraySizeHelper( T ( & )[ N ] ) )[ N ];

#define ARRAY_SIZE( A ) ( sizeof( ArraySizeHelper( A ) ) )

using byte = unsigned char;
