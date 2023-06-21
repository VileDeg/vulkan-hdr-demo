#include "stdafx.h"
#include "Camera.h"
#include "engine.h"

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

    float deltaTime = _dt;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        deltaTime *= _sprintBoost;
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        goFront(deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        goBack(deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        goLeft(deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        goRight(deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        goUp(deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        goDown(deltaTime);
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

glm::mat4 Camera::GetProjMat(float fovY, int w, int h) {
    glm::mat4 projMat = _oldProjMat;

    if (_oldFovY != fovY || _oldWinWidth != w || _oldWinHeight != h) { // If window dimensions or fov changed
        projMat = glm::perspective(glm::radians(fovY), w / (float)h, 0.01f, 200.f);
        projMat[1][1] *= -1; //Flip y-axis
    }

    _oldFovY = fovY;
    _oldWinWidth = w;
    _oldWinHeight = h;
    _oldProjMat = projMat;

    return projMat;
}
