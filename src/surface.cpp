#include "stdafx.h"
#include "Enigne.h"

void Engine::createSurface()
{
    VKASSERTMSG(glfwCreateWindowSurface(_instance, _window, nullptr, &_surface),
        "GLFW: Failed to create window surface");
}