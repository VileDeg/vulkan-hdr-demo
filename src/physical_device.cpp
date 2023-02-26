#include "Enigne.h"

#include <string>
#include <cstring>
#include <array>
#include <vector>
#include <algorithm>

bool Engine::checkDeviceExtensionSupport(VkPhysicalDevice pd)
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensionProperties(extensionCount);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &extensionCount, availableExtensionProperties.data());

    bool allSupported = true;
    for (const auto& de : _deviceExtensions) {
        bool found = false;
        for (VkExtensionProperties& property : availableExtensionProperties) {
            if (strcmp(property.extensionName, de) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            allSupported = false;
    }

    return allSupported;
}

std::vector<std::tuple<VkPhysicalDevice, uint32_t, uint32_t, VkPhysicalDeviceProperties>>
Engine::findCompatibleDevices()
{
    // find compatible devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> deviceList(deviceCount);
    vkEnumeratePhysicalDevices(_instance, &deviceCount, deviceList.data());

    std::vector<std::tuple<VkPhysicalDevice, uint32_t, uint32_t, VkPhysicalDeviceProperties>> compatibleDevices;
    for (VkPhysicalDevice pd : deviceList) {
        
        if (!checkDeviceExtensionSupport(pd))
            continue;

        VkPhysicalDeviceProperties deviceProperties{};
        vkGetPhysicalDeviceProperties(pd, &deviceProperties);

        // select queues for graphics rendering and for presentation
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, queueFamilyList.data());

        uint32_t graphicsQueueFamily = UINT32_MAX;
        uint32_t presentQueueFamily = UINT32_MAX;
        for (uint32_t i = 0, c = uint32_t(queueFamilyList.size()); i < c; i++) {

            // test for presentation support
            VkBool32 presentationSupported = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, _surface, &presentationSupported);
            if (presentationSupported) {

                // test for graphics operations support
                if (queueFamilyList[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    // if presentation and graphics operations are supported on the same queue,
                    // we will use single queue

                    compatibleDevices.emplace_back(pd, i, i, deviceProperties);
                    goto nextDevice;
                }
                else
                    // if only presentation is supported, we store the first such queue
                    if (presentQueueFamily == UINT32_MAX)
                        presentQueueFamily = i;
            }
            else {
                if (queueFamilyList[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    // if only graphics operations are supported, we store the first such queue
                    if (graphicsQueueFamily == UINT32_MAX)
                        graphicsQueueFamily = i;
            }
        }

        if (graphicsQueueFamily != UINT32_MAX && presentQueueFamily != UINT32_MAX)
            // presentation and graphics operations are supported on the different queues
            compatibleDevices.emplace_back(pd, graphicsQueueFamily, presentQueueFamily, deviceProperties);
    nextDevice:;
    }

    return compatibleDevices;
}

void Engine::pickPhysicalDevice()
{
    std::vector<std::tuple<VkPhysicalDevice, uint32_t, uint32_t, VkPhysicalDeviceProperties>>
        compatibleDevices = findCompatibleDevices();
    ASSERTMSG(!compatibleDevices.empty(), "No compatible devices found");

    // print compatible devices
#ifndef NDEBUG
    std::cout << "Compatible devices:" << std::endl;
    for (auto& t : compatibleDevices)
        std::cout << "   " << get<3>(t).deviceName << " (graphics queue: " << get<1>(t)
        << ", presentation queue: " << get<2>(t)
        << ", type: " << std::to_string(get<3>(t).deviceType) << ")" << std::endl;
#endif // NDEBUG

    // choose the best device
    auto bestDevice = compatibleDevices.begin();
    ASSERTMSG(bestDevice != compatibleDevices.end(), "No compatible devices found");

    constexpr const std::array deviceTypeScore = {
        10, // VK_PHYSICAL_DEVICE_TYPE_OTHER         - lowest score
        40, // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU - high score
        50, // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   - highest score
        30, // VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU    - normal score
        20, // VK_PHYSICAL_DEVICE_TYPE_CPU           - low score
        10, // unknown VkPhysicalDeviceType
    };
    int bestScore = deviceTypeScore[std::clamp(int(get<3>(*bestDevice).deviceType), 0, int(deviceTypeScore.size()) - 1)];
    if (get<1>(*bestDevice) == get<2>(*bestDevice))
        bestScore++;
    for (auto it = compatibleDevices.begin() + 1; it != compatibleDevices.end(); it++) {
        int score = deviceTypeScore[std::clamp(int(get<3>(*it).deviceType), 0, int(deviceTypeScore.size()) - 1)];
        if (get<1>(*it) == get<2>(*it))
            score++;
        if (score > bestScore) {
            bestDevice = it;
            bestScore = score;
        }
    }
#ifndef NDEBUG
    std::cout << "Using device:\n"
        "   " << get<3>(*bestDevice).deviceName << std::endl;
#endif // NDEBUG

    _physicalDevice = get<0>(*bestDevice);
    _graphicsQueueFamily = get<1>(*bestDevice);
    _presentQueueFamily = get<2>(*bestDevice);
}
