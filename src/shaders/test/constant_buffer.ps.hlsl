struct PS_INPUT
{
    float3 colour : Colour;
};

struct PSData
{
    float3 color;
};
ConstantBuffer<PSData> PsData_cbv : register(space0);
 
float4 main(PS_INPUT input) : SV_Target0
{
    return float4(PsData_cbv.color, 0.0);
}