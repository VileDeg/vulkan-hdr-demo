#include "stdafx.h"
#include "Camera.h"
#include "Engine.h"

void Camera::calculateFPS()
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

void Camera::calculateDeltaTime()
{
    static float sLastFrameTime = 0.0f;
    float currentTime = glfwGetTime();
    _dt = currentTime - sLastFrameTime;
    sLastFrameTime = currentTime;
}

void Camera::Update(GLFWwindow* window)
{
    calculateDeltaTime();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        goFront(_dt);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        goBack(_dt);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        goLeft(_dt);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        goRight(_dt);
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        goUp(_dt);
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        goDown(_dt);
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
}
