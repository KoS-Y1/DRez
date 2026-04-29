//
// Created by y1 on 2026-04-28.
//

#pragma once

#include <directx/d3d12.h>
#include <directxmath.h>

struct VertexEmpty {
    VertexEmpty() = default;
    static const D3D12_INPUT_LAYOUT_DESC GetInputLayout();
};

struct VertexPT2D {
    DirectX::XMFLOAT2 position;
    DirectX::XMFLOAT2 uv;

    VertexPT2D() = default;

    VertexPT2D(DirectX::XMFLOAT2 position, DirectX::XMFLOAT2 uv)
        : position(position)
        , uv(uv) {}

    static const D3D12_INPUT_LAYOUT_DESC GetInputLayout();
};