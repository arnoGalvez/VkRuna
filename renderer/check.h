// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/vulkan/vulkan.hpp"
#include "platform/Check.h"

#include <stdexcept>
#include <string>

namespace vkRuna
{
namespace render
{
const char *ErrorToString( VkResult result );
}
} // namespace vkRuna

#define VK_CHECK( x )                                                                                      \
	{                                                                                                      \
		VkResult result = x;                                                                               \
		if ( result != VK_SUCCESS )                                                                        \
		{                                                                                                  \
			throw std::runtime_error(                                                                      \
				std::string( __FILE__ " ( " STRINGIFY_MACRO( __LINE__ ) " ):\n Vulkan error message: " ) + \
				std::string( vkRuna::render::ErrorToString( result ) ) );                                  \
		}                                                                                                  \
	}

#define VK_CHECK_PRED( x, pred )                                                                       \
	{                                                                                                  \
		VkResult result	   = x;                                                                        \
		bool	 predValue = pred;                                                                     \
		if ( ( result != VK_SUCCESS ) || !predValue )                                                  \
		{                                                                                              \
			if ( result != VK_SUCCESS )                                                                \
				throw std::runtime_error( std::string( __FILE__ " ( " ) + std::to_string( __LINE__ ) + \
										  std::string( " ):\n Vulkan error message: " ) +              \
										  std::string( vkRuna::render::ErrorToString( result ) ) );    \
			throw std::runtime_error( __FILE__ " ( " STRINGIFY_MACRO( __LINE__ ) "):\n: " #pred );     \
		}                                                                                              \
	}
