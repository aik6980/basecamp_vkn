StructuredBuffer<Scene_instance_desc> scene_instances : register(t0);
RWStructuredBuffer<Indirect_mesh_task_command> indirect_commands : register(u0);
ConstantBuffer<uint> instance_count_cbv : register(b1);

[numthreads(64, 1, 1)]
void csmain(uint3 dtid : SV_DispatchThreadID)
{
    uint instance_id = dtid.x;
    uint count;
    indirect_commands.GetDimensions(count);
    
    if (instance_id >= count) return;
    
    // For now: simple 1:1 mapping
    // todo: add frustum culling here
    indirect_commands[instance_id] = Indirect_mesh_task_command{
        .m_group_count_x = 1,
        .m_group_count_y = 1,
        .m_group_count_z = 1,
    };
}