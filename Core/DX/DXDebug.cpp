//
// Created by y1 on 2026-05-11.
//

#include "DXDebug.h"

namespace drez::dx::debug {
namespace {
// PIX BeginEvent metadata = 1 -> legacy UTF-16 string payload (recognized by RenderDoc and PIX).
constexpr UINT kPixEventUnicodeVersion = 1;
} // namespace

std::wstring ToWide(std::string_view text) {
    std::wstring wide;
    wide.reserve(text.size());
    for (const char c: text) {
        wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
    }
    return wide;
}

void SetObjectName(ID3D12Object *object, std::string_view name) {
    if (object == nullptr) {
        return;
    }
    const std::wstring wide = ToWide(name);
    object->SetName(wide.c_str());
}

ScopedEvent::ScopedEvent(ID3D12GraphicsCommandList *commandList, std::string_view name)
    : ScopedEvent(commandList, ToWide(name)) {}

ScopedEvent::ScopedEvent(ID3D12GraphicsCommandList *commandList, std::wstring_view name)
    : m_commandList(commandList) {
    if (m_commandList == nullptr) {
        return;
    }
    const UINT byteCount = static_cast<UINT>((name.size() + 1) * sizeof(wchar_t));
    m_commandList->BeginEvent(kPixEventUnicodeVersion, name.data(), byteCount);
}

ScopedEvent::~ScopedEvent() {
    if (m_commandList != nullptr) {
        m_commandList->EndEvent();
    }
}
} // namespace drez::dx::debug
