#include "../hlsl_shared_struct.h"

StructuredBuffer<Scene_instance_desc> scene_instance_srv : register(t0);
RWStructuredBuffer<Indirect_mesh_task_command> indirect_command_uav : register(u1);
RWStructuredBuffer<uint> instance_count_uav : register(u2);
RWStructuredBuffer<uint> visible_instance_ids_uav : register(u3);

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
    bool visible = true;

    if (!visible)
        return;

    uint write_index = 0;
    InterlockedAdd(instance_count_uav[0], 1, write_index);
    visible_instance_ids_uav[write_index] = instance_id;

    // fill in fix command for now
    Indirect_mesh_task_command cmd;
    cmd.m_group_count_x            = 16; // all is visible for now
    cmd.m_group_count_y            = 1;
    cmd.m_group_count_z            = 1;
    cmd.m_draw_id              = 0; // rename this to draw_id, might be useful later
    indirect_command_uav[0] = cmd;
}