implementing Utils;

public float3 Tonemap(float3 hdr)
{
    return hdr / (1.0 + hdr);
}

public float3 LinearToSrgb(float3 color)
{
    float3 low = 12.92 * color;
    float3 high = 1.055 * pow(max(color, 1e-5), 1.0 / 2.4) - 0.055;
    return select(color <= 0.0031308, low, high);
}

public float4 RgbmToLinear(float4 input)
{
    return float4(input.xyz * input.w * 6.0f, 1.0f);
}

