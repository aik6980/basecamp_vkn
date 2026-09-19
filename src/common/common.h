#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <assert.h>
//#include <atltypes.h>
//#include <windows.h>

#if defined(_WIN32)
#include <wrl.h>
#else
struct SDL_Window;
using HINSTANCE = void*;
using HWND = SDL_Window*;
using UINT = unsigned int;

struct CRect {
	long left = 0;
	long top = 0;
	long right = 0;
	long bottom = 0;

	CRect() = default;
	CRect(long left_value, long top_value, long right_value, long bottom_value)
		: left(left_value), top(top_value), right(right_value), bottom(bottom_value)
	{
	}

	long Width() const { return right - left; }
	long Height() const { return bottom - top; }
};
#endif

// important Vulkan-specific note: GLM defaults to OpenGL clip space (depth -1 to 1). Vulkan uses depth 0 to 1.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>    // for translate, rotate, scale
#include <glm/ext/matrix_clip_space.hpp>   // for perspective, ortho
#include <glm/ext/quaternion_common.hpp>   // for quaternion operations
#include <glm/gtc/quaternion.hpp>

using namespace std;

#if defined(_WIN32)
using Microsoft::WRL::ComPtr;
#endif

#include "debug/debug_output.h"
#include "debug/debug_util.h"

#include "helper/helper.h"
#include "helper/math.h"
#include "helper/string_pool.h"

#include "graphic/graphic.h"
#include "math/aabb.h"
