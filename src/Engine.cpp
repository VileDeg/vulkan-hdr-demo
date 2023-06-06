#include "stdafx.h"

#include "Engine.h"
#include "defs.h"

void Engine::Init()
{
    createWindow();
    createInstance();
    createDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    createVmaAllocator();

    createSwapchain();
    createImageViews();
    createRenderPass();

    createFrameData();
    createPipelines();
    createFramebuffers();
    
    createScene();

    initImgui();

    _isInitialized = true;
}

void Engine::Run()
{
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();

        imguiCommands();

        drawFrame();

        _inp.camera.Update(_window);

        
    }

    vkDeviceWaitIdle(_device);
}

void Engine::Cleanup()
{
    if (!_isInitialized) {
        return;
    }
    cleanupSwapchainResources();
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    _deletionStack.flush();
}


