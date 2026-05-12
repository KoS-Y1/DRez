//
// Created by y1 on 2026-05-11.
//

#pragma once

#include <string>
#include <string_view>

#include <directx/d3d12.h>

namespace drez::dx::debug {
std::wstring ToWide(std::string_view text);

void SetObjectName(ID3D12Object *object, std::string_view name);

class ScopedEvent {
public:
    ScopedEvent(ID3D12GraphicsCommandList *commandList, std::string_view name);
    ScopedEvent(ID3D12GraphicsCommandList *commandList, std::wstring_view name);
    ~ScopedEvent();

    ScopedEvent(const ScopedEvent &)            = delete;
    ScopedEvent &operator=(const ScopedEvent &) = delete;
    ScopedEvent(ScopedEvent &&)                 = delete;
    ScopedEvent &operator=(ScopedEvent &&)      = delete;

private:
    ID3D12GraphicsCommandList *m_commandList{};
};
} // namespace drez::dx::debug
