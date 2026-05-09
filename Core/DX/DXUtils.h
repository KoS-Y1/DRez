//
// Created by y1 on 2026-04-30.
//

#pragma once
#include <cstdint>

#include <directx/dxgiformat.h>

namespace drez::dx_utils {
uint32_t GetFormatSize(DXGI_FORMAT format);
bool IsDepthStencilFormat(DXGI_FORMAT format);
}
