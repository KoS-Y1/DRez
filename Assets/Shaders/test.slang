struct VSIn
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD;
}

struct VSOut
{
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD0;
}

[shader("vertex")]
VSOut vertexMain(VSIn input)
{
    return { float4(input.pos, 0.0f, 1.0f), input.uv };
}

[shader("pixel")]
float4 pixelMain(VSOut input) : SV_Target
{
    return float4(0.0f, 0.0f, 1.0f, 1.0f);
}
