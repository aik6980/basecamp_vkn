#include "../hlsl_shared_struct.h"

SamplerState Linear_sam : register(s0);
Texture2D ColourTex_srv : register(t1);

struct CameraData {
    float4x4 m_view;
    float4x4 m_projection;
};
ConstantBuffer<CameraData> Camera_cbv : register(b3);

StructuredBuffer<float3> SceneVertices_srv : register(t4);
StructuredBuffer<uint> SceneIndices_srv : register(t5);

StructuredBuffer<Scene_instance_desc> SceneInstances_srv : register(t6);
StructuredBuffer<Scene_mesh_desc> SceneMeshes_srv : register(t7);
StructuredBuffer<Scene_transform_desc> SceneTransforms_srv : register(t8);

// DrawID workaround
StructuredBuffer<Indirect_mesh_task_command> IndirectCommands_srv : register(t9);
RWStructuredBuffer<uint> Taskgroup_counter_uav : register(u10);

groupshared uint gs_command_index;
groupshared uint gs_instance_id;

struct PS_INPUT {
    float4 position : SV_Position;
    float3 colour : Colour;
    float2 uv_coord : Texcoord0;
};

static const uint k_max_verts = 64;
static const uint k_max_tris  = 64;

[outputtopology("triangle")]
[numthreads(64, 1, 1)]
void msmain(
    uint gtid : SV_GroupThreadID,
    out vertices PS_INPUT verts[k_max_verts],
    out indices uint3 tris[k_max_tris])
{
    // Thread 0: atomically get this taskgroup's command index
    if (gtid == 0) {
        uint cmd_idx = 0;
        InterlockedAdd(Taskgroup_counter_uav[0], 1, cmd_idx);
        gs_command_index = cmd_idx;
        gs_instance_id = IndirectCommands_srv[cmd_idx].m_instance_id;
    }
    
    GroupMemoryBarrierWithGroupSync();

    uint instance_id = gs_instance_id;
    Scene_instance_desc inst = SceneInstances_srv[instance_id];
    Scene_mesh_desc mesh     = SceneMeshes_srv[inst.m_mesh_id];
    Scene_transform_desc xf  = SceneTransforms_srv[inst.m_transform_id];

    uint v_count = mesh.m_num_vertices;
    uint t_count = mesh.m_num_indices / 3;
    uint v_off   = mesh.m_offset_vertices;
    uint i_off   = mesh.m_offset_indices;

    SetMeshOutputCounts(v_count, t_count);

    if (gtid < v_count) {
        float3 position      = SceneVertices_srv[v_off + gtid];
        float4 pos_ws        = mul(float4(position, 1.0), xf.m_obj_to_world);
        float4 pos_vs        = mul(pos_ws, Camera_cbv.m_view);
        verts[gtid].position = mul(pos_vs, Camera_cbv.m_projection);
        verts[gtid].uv_coord = position.xy + 0.5;
        verts[gtid].colour   = float3(1, 1, 1);
    }

    if (gtid < t_count) {
        uint base  = i_off + gtid * 3;
        tris[gtid] = uint3(
            SceneIndices_srv[base + 0],
            SceneIndices_srv[base + 1],
            SceneIndices_srv[base + 2]);
    }
}

float4 psmain(PS_INPUT input) : SV_Target0
{
    float3 tex_color = ColourTex_srv.Sample(Linear_sam, input.uv_coord).rgb;
    return float4(tex_color, 1.0);
}