// Copyright (c) 2021 Arno Galvez

#pragma once

#include <cstdint>

template< typename T >
inline T Align( T x, T alignment )
{
	return ( x + alignment - 1 ) & ~( alignment - 1 );
}

// find last set
constexpr uint64_t fls( uint64_t n )
{
	n |= ( n >> 1 );
	n |= ( n >> 2 );
	n |= ( n >> 4 );
	n |= ( n >> 8 );
	n |= ( n >> 16 );
	n |= ( n >> 32 );
	return n ^ ( n >> 1 );
}

// find first set
constexpr uint64_t ffs( uint64_t n )
{
	n |= ( n << 1 );
	n |= ( n << 2 );
	n |= ( n << 4 );
	n |= ( n << 8 );
	n |= ( n << 16 );
	n |= ( n << 32 );
	return n ^ ( n << 1 );
}

// bit scan reverse
// Index of most significant bit. Caution: 0 and 1 both return 0.
constexpr int bsr( uint64_t n )
{
	int i = 0;
	while ( n >>= 1 )
	{
		++i;
	}
	return i;
}

constexpr uint64_t enumMask( uint64_t second, uint64_t last )
{
	uint64_t sm	  = fls( second ) - 1;
	uint64_t flsl = fls( last );

	return ( flsl | ( flsl - 1 ) ) & ~sm;
}
