//
// Created by y1 on 2026-04-26.
//

#include "Window.h"

#include <SDL3/SDL.h>

#include <unordered_map>

#include "Camera.h"
#include "DXApp.h"
#include "Debug.h"
#include "Renderer.h"

namespace {
constexpr int kMaxWindowWidth{2040};
constexpr int kMaxWindowHeight{1080};

constexpr DirectX::XMFLOAT3 kForward{0.0f, 0.0f, -1.0f};
constexpr DirectX::XMFLOAT3 kBackward{0.0f, 0.0f, 1.0f};
constexpr DirectX::XMFLOAT3 kRight{1.0f, 0.0f, 0.0f};
constexpr DirectX::XMFLOAT3 kLeft{-1.0f, 0.0f, 0.0f};
constexpr DirectX::XMFLOAT3 kUp{0.0f, 1.0f, 0.0f};
constexpr DirectX::XMFLOAT3 kDown{0.0f, -1.0f, 0.0f};
} // namespace

Window::Window()
    : m_width(kMaxWindowWidth)
    , m_height(kMaxWindowHeight) {
    m_window = SDL_CreateWindow("DRez", m_width, m_height, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    DebugCheckCritical(m_window != nullptr, "Failed to create window");

    m_dxApp = std::make_unique<DXApp>(GetHWND());
}

Window::~Window() {
    m_dxApp.reset();
}

void Window::Run() {
    Camera camera;
    m_renderer = std::make_unique<Renderer>(*m_dxApp, camera);

    DebugInfo("SDL_Window running");

    auto processInput = [&camera](const SDL_Event &event) {
        static const std::unordered_map<SDL_Scancode, DirectX::XMFLOAT3> cameraMovement{
            {SDL_SCANCODE_W, kForward },
            {SDL_SCANCODE_S, kBackward},
            {SDL_SCANCODE_D, kRight   },
            {SDL_SCANCODE_A, kLeft    },
            {SDL_SCANCODE_Q, kDown      },
            {SDL_SCANCODE_E, kUp    },
        };

        // Camera movement
        if (event.type == SDL_EVENT_KEY_DOWN) {
            const auto pair = cameraMovement.find(event.key.scancode);
            if (pair != cameraMovement.end()) {
                camera.ProcessMovement(pair->second);
            }
        }

        // Camera rotation
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            SDL_Keymod mod = SDL_GetModState();
            if (mod & SDL_KMOD_SHIFT) {
                DirectX::XMFLOAT2 offset{event.motion.xrel, event.motion.yrel};
                camera.ProcessRotation(offset);
            }
        }

    };

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
            processInput(event);
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