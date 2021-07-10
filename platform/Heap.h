// Copyright (c) 2021 Arno Galvez

#pragma once

#include <cstddef>

void *operator new( std::size_t count );
void  operator delete( void *ptr ) noexcept;