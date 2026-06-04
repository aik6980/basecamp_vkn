RWTexture2D<float4> Colour_uav : register(u0);

[numthreads(8, 8, 1)]
void csmain(uint3 tid : SV_DispatchThreadID)
{
    float2 uv = (float2(tid.xy) + 0.5) / float2(256.0, 256.0);
    Colour_uav[tid.xy] = float4(uv, 0.0, 1.0);
}