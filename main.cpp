//
// Created by y1 on 2026-04-26.
//

#include "ResourceManager.h"
#include "Window.h"

int main() {
    Window window{};

    ResourceManager::GetInstance().Init(*window.GetDXApp());
    window.Run();

    return 0;
}
