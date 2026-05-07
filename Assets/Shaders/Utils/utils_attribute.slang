implementing Utils;

import ShaderIO;

// Retrive vertex attribute
public __generic<T : IFloat> T GetVertexAttribute(uint8_t *dataBufferAddress, shader_io::BufferView bufferView,
                                                  uint attributeIndex)
{
    if (bufferView.count > 0)
    {
        T *ptr = (T *)(dataBufferAddress + bufferView.offset + attributeIndex * bufferView.byteStride);
        return ptr[0];
    }

    return T(0);
}

// Interpolate vertex attributes across a triangle using barycentric coordinates
public __generic<T : IFloat> T GetTriangleAttribute(uint8_t *dataBufferAddress, shader_io::BufferView bufferView,
                                                    uint3 attributeIndex, float3 barycentrics)
{
    T attribute0 = GetVertexAttribute<T>(dataBufferAddress, bufferView, attributeIndex.x);
    T attribute1 = GetVertexAttribute<T>(dataBufferAddress, bufferView, attributeIndex.y);
    T attribute2 = GetVertexAttribute<T>(dataBufferAddress, bufferView, attributeIndex.z);

    return T(barycentrics.x) * attribute0 + T(barycentrics.y) * attribute1 + T(barycentrics.z) * attribute2;
}

public int3 GetTriangleIndices(uint8_t *dataBufferAddress, const shader_io::TriangleMesh mesh, int primitiveID)
{
    if (mesh.indices.byteStride == sizeof(int16_t))
    {
        int16_t3 *indices = (int16_t3 *)(dataBufferAddress + mesh.indices.offset);
        return (indices[primitiveID]);
    }
    else if (mesh.indices.byteStride == sizeof(int32_t))
    {
        int32_t3 *indices = (int32_t3 *)(dataBufferAddress + mesh.indices.offset);
        return (indices[primitiveID]);
    }
    else
    {
        // Invalid index format
        return int3(-1);
    }
}
