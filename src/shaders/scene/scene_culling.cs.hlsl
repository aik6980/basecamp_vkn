#include "../hlsl_shared_struct.h"

StructuredBuffer<Scene_instance_desc> scene_instance_srv : register(t0);
RWStructuredBuffer<Indirect_mesh_task_command> indirect_command_uav : register(u1);
RWStructuredBuffer<uint> instance_count_uav : register(u2);

[numthreads(64, 1, 1)] 
void csmain(uint3 dtid : SV_DispatchThreadID) 
{
    uint instance_id = dtid.x;
    uint count       = 0;
    uint stride      = 0;
    scene_instance_srv.GetDimensions(count, stride);

    if (instance_id >= count)
        return;

    // TODO: visibility test; currently all visible
    uint write_index = 0;
    InterlockedAdd(instance_count_uav[0], 1, write_index);
    
    // For now: simple 1:1 mapping
    // todo: add frustum culling here
    Indirect_mesh_task_command cmd;
    cmd.m_group_count_x            = 1;
    cmd.m_group_count_y            = 1;
    cmd.m_group_count_z            = 1;
    cmd.m_instance_id              = instance_id;
    indirect_command_uav[write_index] = cmd;
}