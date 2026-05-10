implementing Utils;

public float3 Tonemap(float3 hdr)
{
    return hdr / (1.0 + hdr);
}

public float3 LinearToSrgb(float3 color)
{
    float3 low = 12.92 * color;
    float3 high = .055 * pow(max(color, 1e-5), 1.0 / 2.4) - 0.055;
    return select(color <= 0.0031308, low, high);
}
