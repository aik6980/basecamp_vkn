// Shader Resource collection
SamplerState Linear_sam : register(s0);
Texture2D ColourTex_srv : register(t1);

struct WorldData {
    float4x4 m_world;
};
ConstantBuffer<WorldData> World_cbv : register(b2);

struct CameraData {
    float4x4 m_view;
    float4x4 m_projection;
};
ConstantBuffer<CameraData> Camera_cbv : register(b3);

// Stage Data
struct PS_INPUT {
    float4 position : SV_Position;
    float3 colour : Colour;
    float2 uv_coord : Texcoord0;
};

// Add new bindings
StructuredBuffer<float3> SceneVertices_srv : register(t4);
StructuredBuffer<uint>   SceneIndices_srv  : register(t5);

struct MeshInfo {
    uint m_vertex_count;
    uint m_index_count;
    uint m_vertex_offset;
    uint m_index_offset;
};
ConstantBuffer<MeshInfo> MeshInfo_cbv : register(b6);

// Max output sizes (compile-time constants, set to meshlet capacity)
static const uint k_max_verts = 64;
static const uint k_max_tris  = 64;

// Mesh Shader
[outputtopology("triangle")]
[numthreads(64, 1, 1)] 
void msmain(
    uint gtid : SV_GroupThreadID, 
    out vertices PS_INPUT verts[k_max_verts], 
    out indices uint3 tris[k_max_tris])
{
    uint v_count = MeshInfo_cbv.m_vertex_count;
    uint t_count = MeshInfo_cbv.m_index_count / 3;
    uint v_off   = MeshInfo_cbv.m_vertex_offset;
    uint i_off   = MeshInfo_cbv.m_index_offset;

    SetMeshOutputCounts(v_count, t_count);

    if (gtid < v_count) {
        float3 pos           = SceneVertices_srv[v_off + gtid];
        float4 pos_ws        = mul(float4(pos, 1.0), World_cbv.m_world);
        float4 pos_vs        = mul(pos_ws, Camera_cbv.m_view);
        verts[gtid].position = mul(pos_vs, Camera_cbv.m_projection);
        verts[gtid].uv_coord = pos.xy + float2(0.5, 0.5);
        verts[gtid].colour   = float3(1, 1, 1);
    }

    if (gtid < t_count) {
        uint base  = i_off + gtid * 3;
        tris[gtid] = uint3(SceneIndices_srv[base + 0], SceneIndices_srv[base + 1], SceneIndices_srv[base + 2]);
    }
}

// Pixel Shader
float4 psmain(PS_INPUT input)
    : SV_Target0
{
    float3 tex_color = ColourTex_srv.Sample(Linear_sam, input.uv_coord).rgb;
    return float4(tex_color, 1.0);
}