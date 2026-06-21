// Shader Resource collection
SamplerState Linear_sam : register(s0);
Texture2D ColourTex_srv : register(t1);

struct WorldData
{
    float4x4 m_world;
};
ConstantBuffer<WorldData> World_cbv : register(b2);

// Stage Data
struct PS_INPUT {
    float4 position : SV_Position;
    float3 colour : Colour;
    float2 uv_coord : Texcoord0;
};

// Mesh Shader
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void msmain(out vertices PS_INPUT verts[3], out indices uint3 tris[1])
{
    SetMeshOutputCounts(3, 1);

    float3 positions[3] = {
        float3(0.0, -0.5, 0.0),
        float3(0.5,  0.5, 0.0),
        float3(-0.5, 0.5, 0.0),
    };

    for (uint i = 0; i < 3; ++i) {
        float4 pos_ws = mul(float4(positions[i], 1.0), World_cbv.m_world);
        verts[i].position = pos_ws;
        verts[i].uv_coord = positions[i].xy + float2(0.5, 0.5);
    }

    verts[0].colour = float3(1.0, 0.0, 0.0);
    verts[1].colour = float3(0.0, 1.0, 0.0);
    verts[2].colour = float3(0.0, 0.0, 1.0);

    tris[0] = uint3(0, 1, 2);
}

// Pixel Shader
float4 psmain(PS_INPUT input) : SV_Target0
{
    float3 tex_color = ColourTex_srv.Sample(Linear_sam, input.uv_coord).rgb;
    return float4(tex_color, 1.0);
}