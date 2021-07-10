// Copyright (c) 2021 Arno Galvez

#include "Heap.h"

#include <cstdlib>

void *operator new( std::size_t count )
{
	auto ptr = malloc( count );
	// TracyAlloc( ptr, count );
	return ptr;
}
void operator delete( void *ptr ) noexcept
{
	// TracyFree( ptr );
	free( ptr );
}
