#include "stdafx.h"
#include "Camera.h"
#include "Enigne.h"

void Engine::calculateFPS()
{
    using namespace std::chrono;

    static int sFpsCount = 0;
    static time_point<steady_clock> sLastTime = steady_clock::now();

    auto currentTime = steady_clock::now();

    const auto diff = currentTime - sLastTime;
    const auto elapsedTime = duration_cast<nanoseconds>(diff).count();
    ++sFpsCount;

    if (elapsedTime > 1'000'000'000) {
        sLastTime = currentTime;
        _fps = sFpsCount;
        sFpsCount = 0;
    }
}

void Engine::calculateDeltaTime()
{
    static float sLastFrameTime = 0.0f;
    float currentTime = glfwGetTime();
    _deltaTime = currentTime - sLastFrameTime;
    sLastFrameTime = currentTime;
}

void Engine::updateCamera()
{
    if (glfwGetKey(_window, GLFW_KEY_W) == GLFW_PRESS) {
        _camera.goFront(_deltaTime);
    }
    if (glfwGetKey(_window, GLFW_KEY_S) == GLFW_PRESS) {
        _camera.goBack(_deltaTime);
    }
    if (glfwGetKey(_window, GLFW_KEY_A) == GLFW_PRESS) {
        _camera.goLeft(_deltaTime);
    }
    if (glfwGetKey(_window, GLFW_KEY_D) == GLFW_PRESS) {
        _camera.goRight(_deltaTime);
    }
    if (glfwGetKey(_window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        _camera.goUp(_deltaTime);
    }
    if (glfwGetKey(_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(_window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        _camera.goDown(_deltaTime);
    }

    //std::cout << _fps << " " << _camera.pos.x << " " << _camera.pos.y << " " << _camera.pos.z << std::endl;
}
