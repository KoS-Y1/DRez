//
// Created by y1 on 2026-05-10.
//

#include "DXShaderResourceView.h"

#include "DXApp.h"

DXShaderResourceView::DXShaderResourceView(DXApp &app, ID3D12Resource *resource, const D3D12_SHADER_RESOURCE_VIEW_DESC &desc)
    : m_index(app.AllocateBindlessIndex()) {
    app.CreateShaderResourceView(resource, m_index, desc);
}
