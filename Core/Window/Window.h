//
// Created by y1 on 2026-04-26.
//

#pragma once

#include <memory>

#include <Windows.h>

struct SDL_Window;

class DXApp;

class Window {
public:
    Window();

    Window(Window const &)             = delete;
    Window &operator=(Window const &)  = delete;
    Window(Window const &&)            = delete;
    Window &operator=(Window const &&) = delete;

    ~Window();

    void Run();

private:
    SDL_Window *m_window{};
    int         m_width{};
    int         m_height{};
    bool        m_running{false};

    std::unique_ptr<DXApp> m_dxApp;

    [[nodiscard]] HWND GetHWND() const;
};
