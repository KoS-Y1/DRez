//
// Created by y1 on 2026-04-26.
//


#include "Debug.h"
#include "ResourceManager.h"
#include "Window.h"

#include <SDL3/SDL_init.h>

int main() {
    DebugCheckCritical(SDL_Init(SDL_INIT_VIDEO),"Failed to initialize SDL");
    atexit(SDL_Quit);

    Window window{};

    ResourceManager::GetInstance().Init(*window.GetDXApp());
    window.Run();

    return 0;
}
