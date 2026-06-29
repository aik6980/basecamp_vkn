// Shader Resource collection
struct Data
{
    float offset_x;
    float offset_y;
};
ConstantBuffer<Data> Data_cbv : register(b0);

struct PSData
{
    float3 color;
};
ConstantBuffer<PSData> PsData_cbv : register(b1);

// VS
struct VS_OUTPUT
{
    float4 position : SV_Position;
    float3 colour : Colour;
};

static const float2 VPosition[] =
{
    float2(0.0, 0.25),
    float2(0.25, -0.25),
    float2(-0.25, -0.25),
}; 

static const float3 VColour[] =
{
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
};


VS_OUTPUT vsmain(uint vertex_id : SV_VertexID)
{
    VS_OUTPUT output;
    output.position = float4(VPosition[vertex_id] + float2(Data_cbv.offset_x, Data_cbv.offset_y), 0.0, 1.0);
    output.colour = VColour[vertex_id];

    return output;
}

// PS
struct PS_INPUT
{
    float3 colour : Colour;
};
 
float4 psmain(PS_INPUT input) : SV_Target0
{
    return float4(PsData_cbv.color, 0.0);
}

