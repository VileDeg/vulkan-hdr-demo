#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include <vector>
#include <iostream>

struct GLFWwindow;

class Engine
{
public:
    void Init();
    void Draw();
    void Run();
private:
    void initInstance();
    void initPhysicalDevice();
    void initLogicalDevice();
    void initWindow();

private:
    vk::UniqueInstance _instance;
    //vk::UniqueDebugUtilsMessengerEXT _debugMessanger;
    vk::PhysicalDevice _physicalDevice;
    uint32_t _graphicsQueueFamily;
    uint32_t _presentationQueueFamily;

    vk::UniqueSurfaceKHR _surface;
    vk::UniqueDevice _device;

    GLFWwindow* _window;
public:
#ifdef NDEBUG
    static const bool enableValidationLayers = false;
#else
    static const bool enableValidationLayers = true;
#endif
};

