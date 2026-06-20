struct PS_INPUT {
    float4 position : SV_Position;
    float3 colour : Colour;
};

// clang-format off
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void msmain(out vertices PS_INPUT verts[3], out indices uint3 tris[1])
// clang-format on
{
    SetMeshOutputCounts(3, 1);

    verts[0].position = float4(0.0, -0.5, 0.0, 1.0);
    verts[1].position = float4(0.5, 0.5, 0.0, 1.0);
    verts[2].position = float4(-0.5, 0.5, 0.0, 1.0);

    verts[0].colour = float3(1.0, 0.0, 0.0);
    verts[1].colour = float3(0.0, 1.0, 0.0);
    verts[2].colour = float3(0.0, 0.0, 1.0);

    tris[0] = uint3(0, 1, 2);
}

float4 psmain(PS_INPUT input)
    : SV_Target0
{
    return float4(input.colour, 1.0);
}