#include "../hlsl_shared_struct.h"

//StructuredBuffer<Scene_instance_desc> scene_instances : register(t0);
RWStructuredBuffer<Indirect_mesh_task_command> indirect_commands : register(u0);
//ConstantBuffer<uint> instance_count_cbv : register(b1);

[numthreads(64, 1, 1)] 
void csmain(uint3 dtid : SV_DispatchThreadID) 
{
    uint instance_id = dtid.x;
    uint count       = 0;
    uint stride      = 0;
    indirect_commands.GetDimensions(count, stride);

    if (instance_id >= count)
        return;
    
    // For now: simple 1:1 mapping
    // todo: add frustum culling here
    Indirect_mesh_task_command cmd;
    cmd.m_group_count_x            = 1;
    cmd.m_group_count_y            = 1;
    cmd.m_group_count_z            = 1;
    indirect_commands[instance_id] = cmd;
}