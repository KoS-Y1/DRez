//
// Created by y1 on 2026-04-26.
//

#include "Window.h"

#include <SDL3/SDL.h>

#include "DXApp.h"
#include "Debug.h"
#include "Renderer.h"

namespace {
constexpr int kMaxWindowWidth{1600};
constexpr int kMaxWindowHeight{900};
constexpr int kMinWindowWidth{640};
constexpr int kMinWindowHeight{360};
} // namespace

Window::Window()
    : m_width(kMaxWindowWidth)
    , m_height(kMaxWindowHeight) {
    m_window = SDL_CreateWindow("DRez", m_width, m_height, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    DebugCheckCritical(m_window != nullptr, "Failed to create window");

    SDL_SetWindowMaximumSize(m_window, kMaxWindowWidth, kMaxWindowHeight);
    SDL_SetWindowMinimumSize(m_window, kMinWindowWidth, kMinWindowHeight);

    m_dxApp = std::make_unique<DXApp>(GetHWND());
}

Window::~Window() {
    m_dxApp.reset();
}

void Window::Run() {
    m_renderer = std::make_unique<Renderer>(*m_dxApp);

    DebugInfo("SDL_Window running");

    m_running = true;
    while (m_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                m_running = false;
                break;
            default:
                break;
            }
        }

        m_renderer->Render();

    }

    DebugInfo("SDL_Window quitting");
}

HWND Window::GetHWND() const {
    SDL_PropertiesID properties = SDL_GetWindowProperties(m_window);
    HWND             hwnd       = static_cast<HWND>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

    DebugCheckCritical(hwnd != nullptr, "Failed to get window handle");
    return hwnd;
}