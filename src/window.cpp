#include "stdafx.h"
#include "engine.h"

static void glfw_error_callback(int error, const char* description)
{
    std::cout << "GLFW Error: " << description << std::endl;
}

static void toggle(bool& x) {
    x = !x;
}

void Engine::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    Engine& eng = *reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    eng._framebufferResized = true;

    /* Need to add 2 drawFrame() calls to render the image while resizing.
     * First call will only recreate the swapchain, second call will render the image. */
    eng.drawFrame();
    eng.drawFrame();

    eng._framebufferResized = false;
}

void Engine::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Engine& eng = *reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE: // Close window
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_C: // Toggle cursor
            toggle(eng._cursorEnabled);
            glfwSetInputMode(window, GLFW_CURSOR,
                eng._cursorEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            break;
        case GLFW_KEY_T: // Toggle tone mapping
            toggle(eng._state.cmp.enableToneMapping);
            break;
        case GLFW_KEY_E: // Toggle exposure
            toggle(eng._state.enableExposure);
            break;
        }
    }
}

void Engine::cursorCallback(GLFWwindow* window, double xpos, double ypos)
{
    Engine& eng = *reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));

    static bool lastCursorState = eng._cursorEnabled;
    static bool firstCall{ true }; // First call after cursor is enabled

    if (!firstCall) {
        if (lastCursorState != eng._cursorEnabled) {
            PRINF("Cursor toggle.");
            firstCall = true;
        } else {
            firstCall = false;
        }
    }

    if (eng._cursorEnabled) {
        return;
    }

    static glm::vec2 lastPos{};

    if (firstCall) {
        /**
        *  If cursor was just enabled simply set lastPos to current pos.
        *  This is needed to prevent camera from cursor from jumping from old position to the new one instantly.
        */
        firstCall = false;
        lastPos = { xpos, ypos };
        lastCursorState = eng._cursorEnabled;
        return;
    }

    //glm::ivec2 extent = { eng.__windowExtent.width, eng.__windowExtent.height };
    glm::vec2 currPos = { xpos, ypos };

    static float sens = 0.1f;
    glm::vec2 diff = currPos - lastPos;
    diff *= sens;

    //std::cout << V2PR(currPos) << V2PR(lastPos) << V2PR(diff) << std::endl;

    eng._camera.MouseInput(-diff.x, diff.y);

    lastPos = currPos;
    lastCursorState = eng._cursorEnabled;
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
    //inp.cursorEnabled = false;

    glfwSetWindowUserPointer(_window, this); // Make window data accessible inside callbacks

    /* Set callbacks. */
    glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
    glfwSetKeyCallback(_window, keyCallback);
    glfwSetCursorPosCallback(_window, cursorCallback);
}
