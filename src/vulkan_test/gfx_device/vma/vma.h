#pragma once

#if defined(_WIN32)
#include <vma/vk_mem_alloc.h>
#else
#include <vk_mem_alloc.h>
#endif
// clangformat partition: need to include after vk_mem_alloc.h
#if !defined(VULKAN_HPP_DISPATCH_LOADER_STATIC_TYPE)
#define VULKAN_HPP_DISPATCH_LOADER_STATIC_TYPE VULKAN_HPP_DEFAULT_DISPATCHER_TYPE
#endif
#include "vk_mem_alloc.hpp"
