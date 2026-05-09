implementing Utils;

import ShaderIO;

// Retrive vertex attribute
public __generic<T : IFloat> T GetVertexAttribute(ByteAddressBuffer buffer, shader_io::BufferView bufferView,
                                                  uint attributeIndex)
{
    if (bufferView.count > 0)
    {
        return buffer.Load<T>(bufferView.offset + attributeIndex * bufferView.byteStride);
    }

    return T(0);
}

// Interpolate vertex attributes across a triangle using barycentric coordinates
public __generic<T : IFloat> T GetTriangleAttribute(ByteAddressBuffer buffer, shader_io::BufferView bufferView,
                                                    uint3 attributeIndex, float3 barycentrics)
{
    T attribute0 = GetVertexAttribute<T>(buffer, bufferView, attributeIndex.x);
    T attribute1 = GetVertexAttribute<T>(buffer, bufferView, attributeIndex.y);
    T attribute2 = GetVertexAttribute<T>(buffer, bufferView, attributeIndex.z);

    return T(barycentrics.x) * attribute0 + T(barycentrics.y) * attribute1 + T(barycentrics.z) * attribute2;
}

public int3 GetTriangleIndices(ByteAddressBuffer buffer, const shader_io::TriangleMesh mesh, int primitiveID)
{
    if (mesh.indices.byteStride == sizeof(int16_t))
    {
        return int3(buffer.Load<int16_t3>(mesh.indices.offset + primitiveID * sizeof(int16_t3)));
    }
    else if (mesh.indices.byteStride == sizeof(int32_t))
    {
        return buffer.Load<int32_t3>(mesh.indices.offset + primitiveID * sizeof(int32_t3));
    }
    else
    {
        // Invalid index format
        return int3(-1);
    }
}
