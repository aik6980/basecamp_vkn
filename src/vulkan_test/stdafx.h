#pragma once

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <map>
#include <vector>

#include <assert.h>
#if defined(_WIN32)
#include <atltypes.h>
#include <windows.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif

#include <codecvt>

#include "common/common.h"

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#if !defined(_WIN32)
#define VK_NULL_HANDLE nullptr
#endif
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#if defined(None)
#undef None
#endif

#define SPIRV_REFLECT_ENABLE_ASSERTS
#include "spirv_reflect.h"
