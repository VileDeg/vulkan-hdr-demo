#include "stdafx.h"
#include "Enigne.h"

static void glfw_error_callback(int error, const char* description)
{
    std::cout << "GLFW Error: " << description << std::endl;
}

void Engine::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    app->_framebufferResized = true;

    //Need to add 2 drawFrame() calls to render the image while resizing. 
    //First call will only recreate the swapchain, second call will render the image.
    app->drawFrame();
    app->drawFrame();
}

void Engine::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    Camera& cam = app->_camera;
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_C: //Toggle cursor
            app->_cursorEnabled = !app->_cursorEnabled;
            glfwSetInputMode(window, GLFW_CURSOR, 
                app->_cursorEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            break;
        }
    }
}

void Engine::cursorCallback(GLFWwindow* window, double xpos, double ypos)
{
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));

    static bool lastCursorState = app->_cursorEnabled;
    static bool firstCall{ true };

    if (!firstCall) {
        if (lastCursorState != app->_cursorEnabled) {
            std::cout << "Cursor toggle." << std::endl;
            firstCall = true;
        } else {
            firstCall = false;
        }
    }

    if (app->_cursorEnabled) {
        return;
    }

    static glm::vec2 lastPos{};

    if (firstCall) {
        firstCall = false;
        lastPos = { xpos, ypos };
        lastCursorState = app->_cursorEnabled;
        return;
    }
    
    glm::ivec2 extent = { app->_windowExtent.width, app->_windowExtent.height };
    glm::vec2 currPos = { xpos, ypos };

    static float sens = 0.1f;
    glm::vec2 diff = currPos - lastPos;
    diff *= sens;

    //std::cout << V2PR(currPos) << V2PR(lastPos) << V2PR(diff) << std::endl;

    app->_camera.MouseInput(-diff.x, diff.y);

    lastPos = currPos;
    lastCursorState = app->_cursorEnabled;
}

void Engine::createWindow()
{
    glfwSetErrorCallback(glfw_error_callback);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWASSERTMSG(glfwVulkanSupported(), "GLFW: Vulkan Not Supported");

    if (ENABLE_FULLSCREEN) {
        _window = glfwCreateWindow(FS_WIDTH, FS_HEIGHT, "Vulkan", glfwGetPrimaryMonitor(), nullptr);
    } else {
        _window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    _cursorEnabled = false;

    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
    glfwSetKeyCallback(_window, keyCallback);
    glfwSetCursorPosCallback(_window, cursorCallback);
}
