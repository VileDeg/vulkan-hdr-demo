#include "stdafx.h"
#include "Engine.h"

static void glfw_error_callback(int error, const char* description)
{
    std::cout << "GLFW Error: " << description << std::endl;
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) 
{
    InputContext* inp = reinterpret_cast<InputContext*>(glfwGetWindowUserPointer(window));
    inp->framebufferResized = true;

    /* Need to add 2 drawFrame() calls to render the image while resizing.
     * First call will only recreate the swapchain, second call will render the image. */
    inp->onFramebufferResize();
    inp->onFramebufferResize();
}

/**
* Callback for handling different keyboard input.
*/
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    InputContext* inp = reinterpret_cast<InputContext*>(glfwGetWindowUserPointer(window));
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE: // Close window
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_C: // Toggle cursor
            inp->cursorEnabled = !inp->cursorEnabled;
            glfwSetInputMode(window, GLFW_CURSOR, 
                inp->cursorEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            break;
        case GLFW_KEY_T: // Toggle tone mapping
            inp->toneMappingEnabled = !inp->toneMappingEnabled;
            break;
        case GLFW_KEY_E: // Toggle tone mapping
            inp->exposureEnabled = !inp->exposureEnabled;
        }
    }
}

/**
* This callbacks handles mouse input for camera rotation.
* 
* Only works when cursor is disabled.
*/
static void cursorCallback(GLFWwindow* window, double xpos, double ypos)
{
    InputContext* inp = reinterpret_cast<InputContext*>(glfwGetWindowUserPointer(window));

    static bool lastCursorState = inp->cursorEnabled;
    static bool firstCall{ true }; // First call after cursor is enabled

    if (!firstCall) {
        if (lastCursorState != inp->cursorEnabled) {
            PRINF("Cursor toggle.");
            firstCall = true;
        } else {
            firstCall = false;
        }
    }

    if (inp->cursorEnabled) {
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
        lastCursorState = inp->cursorEnabled;
        return;
    }
    
    //glm::ivec2 extent = { inp->_windowExtent.width, inp->_windowExtent.height };
    glm::vec2 currPos = { xpos, ypos };

    static float sens = 0.1f;
    glm::vec2 diff = currPos - lastPos;
    diff *= sens;

    //std::cout << V2PR(currPos) << V2PR(lastPos) << V2PR(diff) << std::endl;

    inp->camera.MouseInput(-diff.x, diff.y);

    lastPos = currPos;
    lastCursorState = inp->cursorEnabled;
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

    glfwSetWindowUserPointer(_window, &_inp); // Make window data accessible inside callbacks

    /* Set callbacks. */
    glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
    glfwSetKeyCallback(_window, keyCallback);
    glfwSetCursorPosCallback(_window, cursorCallback);
}
