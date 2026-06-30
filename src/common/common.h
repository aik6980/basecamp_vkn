#pragma once

#include <algorithm>
#include <filesystem>
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

// d3d12
#include <wrl.h>

// important Vulkan-specific note: GLM defaults to OpenGL clip space (depth -1 to 1). Vulkan uses depth 0 to 1.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>    // for translate, rotate, scale
#include <glm/ext/matrix_clip_space.hpp>   // for perspective, ortho
#include <glm/ext/quaternion_common.hpp>   // for quaternion operations
#include <glm/gtc/quaternion.hpp>

using namespace std;

using Microsoft::WRL::ComPtr;

#include "debug/debug_output.h"
#include "debug/debug_util.h"

#include "helper/helper.h"
#include "helper/math.h"
#include "helper/string_pool.h"

#include "graphic/graphic.h"
#include "math/aabb.h"
