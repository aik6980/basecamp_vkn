RWTexture2D<float4> ColourTex_uav : register(u0);

// clang-format off
[numthreads(8, 8, 1)]
void csmain(uint3 tid : SV_DispatchThreadID)
// clang-format on
{
    uint width, height;
    ColourTex_uav.GetDimensions(width, height);

    if (tid.x >= width || tid.y >= height) {
        return;
    }

    float2 uv             = (float2(tid.xy) + 0.5) / float2(width, height);
    ColourTex_uav[tid.xy] = float4(uv.x, uv.y, 0.0, 1.0);
}