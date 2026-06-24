RaytracingAccelerationStructure Scene_srv : register(t0);
RWTexture2D<float4> Output_uav : register(u1);

struct Payload_st
{
    float4 colour;
};

[shader("raygeneration")]
void raygen_main()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;

    float2 uv = (float2(pixel) + 0.5) / float2(dims);
    float2 ndc = uv * 2.0 - 1.0;

    RayDesc ray;
    ray.Origin = float3(0.0, 0.0, -3.0);
    ray.Direction = normalize(float3(ndc.x, -ndc.y, 1.0));
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    Payload_st payload;
    payload.colour = float4(0.0, 0.0, 0.0, 1.0);

    TraceRay(Scene_srv, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 1, 0, ray, payload);
    Output_uav[pixel] = payload.colour;
}

typedef BuiltInTriangleIntersectionAttributes TriAttr;

[shader("closesthit")]
void closethit_main(inout Payload_st payload, in TriAttr attr)
{
    float t = RayTCurrent();
    float g = saturate(1.0 - t / 10.0);
    payload.colour = float4(g, g, g, 1.0);
}

[shader("miss")]
void miss_main(inout Payload_st payload)
{
    payload.colour = float4(0.0, 0.0, 0.0, 1.0);
}