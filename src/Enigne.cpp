#include "stdafx.h"

#include "Enigne.h"
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
    createGraphicsPipeline();
    createFramebuffers();
    //createCommandPool();
    createCommandBuffers();
    createSyncObjects();

    loadMeshes();
    createScene();

    _isInitialized = true;
}

void Engine::Run()
{
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        drawFrame();

        _camera.Update(_window, _deltaTime);
        calculateDeltaTime();
    }

    vkDeviceWaitIdle(_device);
}

void Engine::Cleanup()
{
    if (!_isInitialized) {
        return;
    }

    for (auto& mesh : _meshes) {
        //Destroy vertex buffers
        mesh.second.cleanup(_allocator);
    }
    for (auto& material : _materials) {
        //Destroy pipeline layout and pipeline
        material.second.cleanup(_device);
    }

    //Delete all synchronization objects of frames
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(_device, _frames[i].imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(_device, _frames[i].renderFinishedSemaphore, nullptr);
        vkDestroyFence(_device, _frames[i].inFlightFence, nullptr);

        vkDestroyCommandPool(_device, _frames[i].commandPool, nullptr);
    }
    
    //cleanupPipeline();
    vkDestroyRenderPass(_device, _renderPass, nullptr);

    cleanupSwapchainResources();
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    _deletionStack.flush();

    /*vmaDestroyAllocator(_allocator);

    vkDestroyDevice(_device, nullptr);

    if (ENABLE_VALIDATION_LAYERS) {
        DYNAMIC_LOAD(ddum, _instance, vkDestroyDebugUtilsMessengerEXT);
        ddum(_instance, _debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyInstance(_instance, nullptr);

    glfwDestroyWindow(_window);
    glfwTerminate();*/
}