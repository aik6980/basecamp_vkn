// Shader Resource collection
SamplerState Linear_sam : register(s0);
Texture2D ColourTex_srv : register(t1);


// VS
struct VS_OUTPUT 
{ 
    float4 position : SV_POSITION; 
    float2 uv_coord : TEXCOORD0; 
}; 

VS_OUTPUT vsmain(uint vertex_id : SV_VertexID)
{
    VS_OUTPUT Output; 
    Output.uv_coord = float2((vertex_id << 1) & 2, vertex_id & 2); 
    Output.position = float4(Output.uv_coord * float2(2,-2) + float2(-1,1), 0, 1); 
    
    return Output;
}

float4 psmain(VS_OUTPUT input) : SV_Target0
{
    float3 tex_color = float3(1.0, 1.0, 1.0);

    tex_color = ColourTex_srv.Sample(Linear_sam, input.uv_coord).rgb;

    return float4(tex_color, 1.0);
}
