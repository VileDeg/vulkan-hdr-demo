#include "stdafx.h"
#include "defs.h"
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
    Engine& e = *reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));

    e.recreateSwapchain();
    // Need to reset frames counter otherwise viewport image layout error occurs on window resize
    e._currentFrameInFlight = 0;

    e.drawFrame();
}

void Engine::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Engine& eng = *reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE: // Close window
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
       
        case GLFW_KEY_T: // Start FPS measurment
            if (!eng._measureFPS) {
                eng._measureFPS = true;
                eng._numFramesMeasured = 0;

                pr("[FPS] Measurement for " << MEASURE_FPS_INTERVAL << "s. started");
            }
            break;
        }
    }

    // LEFT CONTROL
    if (
        action == GLFW_PRESS &&
        glfwGetKey(eng._window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) 
    {
        switch (key) {
        case GLFW_KEY_C: // Toggle cursor
            toggle(eng._cursorEnabled);
            glfwSetInputMode(window, GLFW_CURSOR,
                eng._cursorEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            break;
        case GLFW_KEY_N: // Take screenshot of the viewport
            eng._screenshotWindowEn = true;
            break;
        }

        if (eng._cursorEnabled) {
            switch (key) {
            case GLFW_KEY_S:
                eng._saveSceneWindowEn = true;
                eng._loadSceneWindowEn = false;
                break;
            case GLFW_KEY_D:
                eng._loadSceneWindowEn = true;
                eng._saveSceneWindowEn = false;
                break;
            }
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
        // If cursor was just enabled simply set lastPos to current pos.
        // This is needed to prevent camera from cursor from jumping from old position to the new one instantly.
        
        firstCall = false;
        lastPos = { xpos, ypos };
        lastCursorState = eng._cursorEnabled;
        return;
    }

    glm::vec2 currPos = { xpos, ypos };

    static float sens = 0.1f;
    glm::vec2 diff = currPos - lastPos;
    diff *= sens;

    eng._camera.MouseInput(-diff.x, diff.y);

    lastPos = currPos;
    lastCursorState = eng._cursorEnabled;
}



void Engine::createWindow()
{
    glfwSetErrorCallback(glfw_error_callback);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFW_ASSERT_MSG(glfwVulkanSupported(), "GLFW: Vulkan Not Supported");

    if (ENABLE_FULLSCREEN) {
        _window = glfwCreateWindow(FS_WIDTH, FS_HEIGHT, "Vulkan", glfwGetPrimaryMonitor(), nullptr);
    } else {
        _window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetWindowUserPointer(_window, this); // Make window data accessible inside callbacks

    /* Set callbacks. */
    glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
    glfwSetKeyCallback(_window, keyCallback);
    glfwSetCursorPosCallback(_window, cursorCallback);
}
