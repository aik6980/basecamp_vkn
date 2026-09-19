#include "vma.h"

#define VMA_IMPLEMENTATION
#if defined(_WIN32)
#include <vma/vk_mem_alloc.h>
#else
#include <vk_mem_alloc.h>
#endif
// clangformat partition: need to include after vk_mem_alloc.h
#include "vk_mem_alloc.hpp"
