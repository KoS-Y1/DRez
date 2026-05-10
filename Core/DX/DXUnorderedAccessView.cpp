//
// Created by y1 on 2026-05-10.
//

#include "DXUnorderedAccessView.h"

#include "DXApp.h"

DXUnorderedAccessView::DXUnorderedAccessView(DXApp &app, ID3D12Resource *resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC &desc)
    : m_index(app.AllocateBindlessIndex()) {
    app.CreateUnorderedAccessView(resource, m_index, desc);
}
