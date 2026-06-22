#ifndef HLSL_SHARED_STRUCT__HLSL
#define HLSL_SHARED_STRUCT__HLSL

#ifdef COMPILE_CPP
#include <DirectXMath.h>
using namespace DirectX;

using float4x4 = XMFLOAT4X4;
using float4   = XMFLOAT4;
using float3   = XMFLOAT3;
using uint	   = uint32_t;

#define Position_sematic
#define Colour_sematic
#else
#define Position_sematic : Position
#define Colour_sematic : Colour
#endif

struct PC_vertex {
	float4 position Position_sematic;
	float4 colour	Colour_sematic;
};

struct Camera_st {
	float4x4 View;
	float4x4 Projection;
};

// using for Raytracing (Legacy)
struct Fat_vertex {
	float3 m_position;
	float4 m_colour;
	float3 m_normal;
};

struct Scene_mesh_desc {
    uint m_num_vertices;
    uint m_num_indices;
    uint m_offset_vertices;
    uint m_offset_indices;

    float3 m_bounds_center;
    float  m_bounds_radius;
};

struct Scene_material_desc {
    uint m_base_colour_texture;
    uint m_normal_texture;
    uint m_surface_texture;
    uint m_flags;

    float4 m_base_colour_factor;

    float m_metallic;
    float m_roughness;
    uint  m_pad0;
    uint  m_pad1;
};

struct Scene_transform_desc {
    float4x4 m_obj_to_world;
    float4x4 m_world_to_obj;
};

struct Scene_instance_desc {
    uint m_mesh_id;
    uint m_material_id;
    uint m_transform_id;
    uint m_flags;

    uint m_visibility_mask;
    uint m_blas_id;
    uint m_pad0;
    uint m_pad1;
};

#endif
