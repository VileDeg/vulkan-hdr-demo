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

void Camera::Update(GLFWwindow* window, float dt)
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        goFront(dt);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        goBack(dt);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        goLeft(dt);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        goRight(dt);
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        goUp(dt);
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        goDown(dt);
    }
}

void Camera::rotate(float x, float y) {
    _yaw -= x * _rotSpeed;
    _pitch -= y * _rotSpeed;
    
    _pitch = std::clamp(_pitch, -89.f, 89.f);

    _front.x = cos(glm::radians(_yaw)) * cos(glm::radians(_pitch));
    _front.y = sin(glm::radians(_pitch));
    _front.z = sin(glm::radians(_yaw)) * cos(glm::radians(_pitch));
    _front = glm::normalize(_front);

    _right = glm::normalize(glm::cross(_front, sWorldUp));
    _up = glm::normalize(glm::cross(_right, _front));

    //std::cout << V3PR(_front) << V3PR(_right) << V3PR(_up) << std::endl;
}
