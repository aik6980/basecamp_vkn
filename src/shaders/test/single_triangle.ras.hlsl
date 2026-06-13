
// VS
struct VS_OUTPUT {
    float4 position : SV_Position;
    float3 colour : Colour;
};

static const float2 VPosition[] = {
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5),
};

static const float3 VColour[] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
};

VS_OUTPUT vsmain(uint vertex_id : SV_VertexID)
{
    VS_OUTPUT output;
    output.position = float4(VPosition[vertex_id], 0.0, 1.0);
    output.colour   = VColour[vertex_id];

    return output;
}

// PS
struct PS_INPUT {
    float3 colour : Colour;
};

float4 psmain(PS_INPUT input)
    : SV_Target0
{
    return float4(input.colour, 1.0);
}