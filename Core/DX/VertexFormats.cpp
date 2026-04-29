//
// Created by y1 on 2026-04-28.
//

#include "VertexFormats.h"

#include <vector>

const D3D12_INPUT_LAYOUT_DESC VertexEmpty::GetInputLayout() {
    return D3D12_INPUT_LAYOUT_DESC{
        .pInputElementDescs = nullptr,
        .NumElements        = 0,
    };
}

const D3D12_INPUT_LAYOUT_DESC VertexPT2D::GetInputLayout() {
    static const std::vector<D3D12_INPUT_ELEMENT_DESC> elements{
        {.SemanticName         = "POSITION",
         .SemanticIndex        = 0,
         .Format               = DXGI_FORMAT_R32G32_FLOAT,
         .InputSlot            = 0,
         .AlignedByteOffset    = offsetof(VertexPT2D, position),
         .InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
         .InstanceDataStepRate = 0},
        {.SemanticName         = "TEXCOORD",
         .SemanticIndex        = 0,
         .Format               = DXGI_FORMAT_R32G32_FLOAT,
         .InputSlot            = 0,
         .AlignedByteOffset    = offsetof(VertexPT2D, uv),
         .InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
         .InstanceDataStepRate = 0},
    };

    return D3D12_INPUT_LAYOUT_DESC{.pInputElementDescs = elements.data(), .NumElements = static_cast<uint32_t>(elements.size())};
}