#include "stdafx.h"
#include "Enigne.h"

static void glfw_error_callback(int error, const char* description)
{
    std::cout << "GLFW Error: " << description << std::endl;
}

void Engine::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    app->_framebufferResized = true;

    //Need to add 2 drawFrame() calls render the image while resizing. 
    //First call will only recreate the swapchain, second call will render the image.
    app->drawFrame();
    app->drawFrame();
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

    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
}
