// Shader Resource collection
SamplerState Linear_sam : register(s0);

Texture2D ColourTex_srv : register(t1);
Texture2D ColourTexArray_srv[3] : register(t2);
// bindless must be last binding in the set/space
Texture2D ColourTexBindless_srv[] : register(t3);

// VS
struct VS_OUTPUT {
    float4 position : SV_Position;
    float3 colour : Colour;
    float2 uv_coord : Texcoord0;
    uint texture_id : Texcoord1;
    uint texture_source : Texcoord2;
};

static const uint Indices[] = {
    0,
    1,
    2,
    3,
    2,
    1,
};

// Vulkan Default Normalized Screen Coordinate - Similar to DirectX
// X    -1..1   Left...Right
// Y    -1..1   Top ..Bottom
// Z    0..1    Near.. Far
static const float2 Positions[] = {
    float2(-0.5, 0.5),
    float2(-0.5, -0.5),
    float2(0.5, 0.5),
    float2(0.5, -0.5),
};

static const float2 InstPositions[] = {
    float2(-0.5, 0.5),
    float2(-0.5, -0.5),
    float2(0.5, 0.5),
    float2(0.5, -0.5),
};

static const float3 InstColours[] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
    float3(1.0, 0.0, 1.0),
};

VS_OUTPUT vsmain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    VS_OUTPUT output;

    uint idx = Indices[vertex_id];
    // local
    float2 pos_local = Positions[idx];
    float scale      = 0.25f;

    if (instance_id <= 3) {
        uint local_id = instance_id;

        uint grid_x = local_id % 2;
        uint grid_y = local_id / 2;

        float2 grid_origin = float2(-0.75f, 0.35f);
        float2 grid_step   = float2(0.35f, -0.35f);

        float2 grid_offset = float2((float)grid_x, (float)grid_y) * grid_step;
        float2 pos_center  = grid_origin + grid_offset;

        float2 pos_world = pos_local * scale + pos_center;

        output.position       = float4(pos_world, 0.0, 1.0);
        output.uv_coord       = Positions[idx] + float2(0.5, 0.5);
        output.colour         = InstColours[instance_id];
        output.texture_id     = local_id;
        output.texture_source = 0; // bindless texture

        return output;
    }

    if (instance_id == 4) {
        float2 pos_center = float2(0.50f, 0.35f);
        float2 pos_world  = pos_local * scale + pos_center;

        output.position       = float4(pos_world, 0.0, 1.0);
        output.uv_coord       = Positions[idx] + float2(0.5, 0.5);
        output.texture_id     = 0;
        output.texture_source = 1; // single texture
        return output;
    }

    float2 pos_world = pos_local * scale + InstPositions[instance_id];

    output.position = float4(pos_world, 0.0, 1.0);
    output.uv_coord = Positions[idx] + float2(0.5, 0.5);
    output.colour   = InstColours[instance_id];

    output.texture_id     = instance_id;
    output.texture_source = 1; // single texture
    return output;
}

// PS
struct PS_INPUT {
    float4 position : SV_Position;
    float3 colour : Colour;
    float2 uv_coord : Texcoord0;
    uint texture_id : Texcoord1;
    uint texture_source : Texcoord2;
};

float4 psmain(PS_INPUT input)
    : SV_Target0
{
    float3 tex_color = float3(1.0, 1.0, 1.0);

    if (input.texture_source == 0) {
        uint safe_id = min(input.texture_id, 3u);
        tex_color    = ColourTexBindless_srv[safe_id].Sample(Linear_sam, input.uv_coord).rgb;
    }
    else {
        tex_color = ColourTex_srv.Sample(Linear_sam, input.uv_coord).rgb;
    }

    return float4(tex_color, 1.0);
}