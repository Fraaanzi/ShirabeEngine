#include <csignal>

#include <functional>
#include <set>
#include <thread>

#include <platform/platform.h>
#include <base/string.h>
#include <graphicsapi/definitions.h>
#include <resources/resourcetypes.h>
#include <os/applicationenvironment.h>
#include <thread>
#include "vulkan_integration/vulkanimport.h"
#include "vulkan_integration/vulkanenvironment.h"
#include "vulkan_integration/vulkandevicecapabilities.h"
#include "vulkan_integration/wsi/x11surface.h"

namespace engine::vulkan
{
    using namespace engine::os;
    using namespace engine::graphicsapi;

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    namespace Debug
    {
        SHIRABE_DECLARE_LOG_TAG(ValidationLayers);
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    static VKAPI_ATTR VkBool32 VKAPI_CALL __vkValidationLayerReportCallback(
            VkDebugReportFlagsEXT      aFlags,
            VkDebugReportObjectTypeEXT aObjType,
            uint64_t                   aObj,
            size_t                     aLocation,
            int32_t                    aCode,
            char                const *aLayerPrefix,
            char                const *aMsg,
            void                      *aUserData)
    {
        std::string const message =
                CString::format(
                    "[{}][{}({})]@'{}' -> ({}) in layer {}:\n{}",
                    aFlags, aObjType, (void*)aObj, aLocation, aCode, aLayerPrefix, aMsg);
        CLog::Debug(Debug::logTag(), message);

        if( VkDebugReportFlagBitsEXT::VK_DEBUG_REPORT_ERROR_BIT_EXT == (aFlags & VkDebugReportFlagBitsEXT::VK_DEBUG_REPORT_ERROR_BIT_EXT) )
        {
            // raise(SIGTRAP); // Trap execution here within GDB on error... __builtin_trap() would cause SIGKILL, not appreciated...
        }

        return VK_FALSE;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    CVulkanEnvironment::CVulkanEnvironment()
        : mVkState            ({})
        , mResourceStorage    (nullptr)
        , mCurrentFrameContext(nullptr)
    {}
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::createVulkanInstance(std::string const &aName)
    {
        VkApplicationInfo vkApplicationInfo{ };
        vkApplicationInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        vkApplicationInfo.pApplicationName   = aName.c_str();
        vkApplicationInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
        vkApplicationInfo.pEngineName        = "Shirabe Engine";
        vkApplicationInfo.engineVersion      = VK_MAKE_VERSION(0, 0, 1);
        vkApplicationInfo.apiVersion         = VK_API_VERSION_1_1;

        std::vector<char const*> const layers =
        {
#ifdef SHIRABE_DEBUG
            "VK_LAYER_LUNARG_standard_validation"
#endif
        };

        std::vector<char const*> const extensions =
        {
            VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef SHIRABE_PLATFORM_WINDOWS
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined SHIRABE_PLATFORM_LINUX
            VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
#ifdef SHIRABE_DEBUG
            VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#endif
        };

        uint32_t instanceLayerCount     = 0;
        uint32_t instanceExtensionCount = 0;

        //
        // Check Layer support
        //
        std::vector<VkLayerProperties> supportedLayers = {};

        vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        supportedLayers.resize(instanceLayerCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, supportedLayers.data());

        std::set<std::string> layersCopy(layers.begin(), layers.end());
        for(VkLayerProperties const&properties : supportedLayers)
        {
            layersCopy.erase(properties.layerName);
        }

        if(!layersCopy.empty())
        {
            throw CVulkanError("Not all required layers supported.", VkResult::VK_ERROR_LAYER_NOT_PRESENT);
        }

        //
        // Check Extension support
        //
        std::vector<VkExtensionProperties> supportedExtensions = {};

        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
        supportedExtensions.resize(instanceExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, supportedExtensions.data());

        std::set<std::string> extensionsCopy(extensions.begin(), extensions.end());
        for(VkExtensionProperties const&ext : supportedExtensions)
        {
            extensionsCopy.erase(ext.extensionName);
        }

        if(!extensionsCopy.empty())
        {
            throw CVulkanError("Not all required extensions supported.", VkResult::VK_ERROR_LAYER_NOT_PRESENT);
        }

        //
        // Create
        //
        VkInstanceCreateInfo vkInstanceCreatInfo = {};
        vkInstanceCreatInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vkInstanceCreatInfo.pApplicationInfo        = &vkApplicationInfo;
        vkInstanceCreatInfo.enabledLayerCount       = layers.size();
        vkInstanceCreatInfo.ppEnabledLayerNames     = layers.data();
        vkInstanceCreatInfo.enabledExtensionCount   = extensions.size();
        vkInstanceCreatInfo.ppEnabledExtensionNames = extensions.data();
        vkInstanceCreatInfo.flags                   = VkInstanceCreateFlags(0);
        vkInstanceCreatInfo.pNext                   = nullptr;

        VkInstance instance = VK_NULL_HANDLE;

        VkResult result = vkCreateInstance(&vkInstanceCreatInfo, nullptr, &instance);
        if(VK_SUCCESS != result) {
            throw CVulkanError("Failed to create VkInstance.", result);
        }

        mVkState.instanceLayers     = layers;
        mVkState.instanceExtensions = extensions;
        mVkState.instanceCreateInfo = vkInstanceCreatInfo;
        mVkState.instance           = instance;

        //
        // In Debug-mode, hook-in a validation layer report callback
        //
#ifdef SHIRABE_DEBUG
        uint32_t const reportFlags =
                VK_DEBUG_REPORT_ERROR_BIT_EXT       |
                VK_DEBUG_REPORT_WARNING_BIT_EXT     |
                VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
                VK_DEBUG_REPORT_DEBUG_BIT_EXT       |
                VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;

        VkDebugReportCallbackCreateInfoEXT vkDebugReportCallbackCreateInfo ={ };
        vkDebugReportCallbackCreateInfo.sType       = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        vkDebugReportCallbackCreateInfo.flags       = reportFlags;
        vkDebugReportCallbackCreateInfo.pfnCallback = __vkValidationLayerReportCallback;

        auto vkCreateDebugReportCallbackEXT =
                (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
        if(!vkCreateDebugReportCallbackEXT)
        {
            throw CVulkanError("Cannot load vulkan extension function 'vkCreateDebugReportCallbackEXT'", VkResult::VK_ERROR_INITIALIZATION_FAILED);
        }

        VkDebugReportCallbackEXT vkDebugReportCallback = 0;
        result = vkCreateDebugReportCallbackEXT(instance, &vkDebugReportCallbackCreateInfo, nullptr, &vkDebugReportCallback);
        if(VkResult::VK_SUCCESS != result)
        {
            throw CVulkanError("Cannot hook-in debug report callback.", VkResult::VK_ERROR_INITIALIZATION_FAILED);
        }

        mVkState.debugReportCallback = vkDebugReportCallback;
#endif
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
//         void CVulkanEnvironment::createVulkanSurface(
//                 os::SApplicationEnvironment const &aApplicationEnvironment)
//         {
//             VkSurfaceKHR surface{};
//
// #if defined SHIRABE_PLATFORM_WINDOWS
//             VkWin32SurfaceCreateInfoKHR vkWin32SurfaceCreateInfo{};
//             vkWin32SurfaceCreateInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
//             vkWin32SurfaceCreateInfo.hwnd      = reinterpret_cast<HWND>(appEnvironment.primaryWindowHandle);
//             vkWin32SurfaceCreateInfo.hinstance = reinterpret_cast<HINSTANCE>(appEnvironment.instanceHandle);
//             vkWin32SurfaceCreateInfo.flags     = 0;
//             vkWin32SurfaceCreateInfo.pNext     = nullptr;
//
//             auto CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(mVkState.instance, "vkCreateWin32SurfaceKHR");
//             if(!CreateWin32SurfaceKHR)
//             {
//                 throw CVulkanError("Cannot find vulkan function 'vkCreateWin32SurfaceKHR'.", VkResult::VK_ERROR_INITIALIZATION_FAILED);
//             }
//
//             VkResult result = CreateWin32SurfaceKHR(mVkState.instance, &vkWin32SurfaceCreateInfo, nullptr, &surface);
//             if(VkResult::VK_SUCCESS != result)
//             {
//                 throw CVulkanError("Failed to create window surface!", result);
//             }
// #elif defined SHIRABE_PLATFORM_LINUX
// #endif
//
//             mVkState.surface = surface;
//         }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::determinePhysicalDevices()
    {
        VkResult result = VkResult::VK_SUCCESS;

        std::vector<char const*> requiredLayers     ={};
        std::vector<char const*> requiredExtensions ={};
        requiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        std::vector<SVulkanPhysicalDevice> supportedPhysicalDevices = {};

        std::vector<VkPhysicalDevice> vkPhysicalDevices;
        uint32_t                      vkPhysicalDeviceCount = 0;

        result = vkEnumeratePhysicalDevices(mVkState.instance, &vkPhysicalDeviceCount, nullptr);
        if(VkResult::VK_SUCCESS == result)
        {
            vkPhysicalDevices.resize(vkPhysicalDeviceCount);
            vkEnumeratePhysicalDevices(mVkState.instance, &vkPhysicalDeviceCount, vkPhysicalDevices.data());

            for(uint64_t k = 0; k < vkPhysicalDevices.size(); ++k)
            {
                VkPhysicalDevice const &vkPhysicalDevice = vkPhysicalDevices.at(k);

                //
                // Check Physical Device Props and Features
                //
                VkPhysicalDeviceProperties vkPhysicalDeviceProperties = {};
                vkGetPhysicalDeviceProperties(vkPhysicalDevice, &vkPhysicalDeviceProperties);

                VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures = {};
                vkGetPhysicalDeviceFeatures(vkPhysicalDevice, &vkPhysicalDeviceFeatures);

                bool const isDiscreteGPU              = (vkPhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
                bool const supportsGeometryShaders    = vkPhysicalDeviceFeatures.geometryShader;
                bool const supportsTesselationShaders = vkPhysicalDeviceFeatures.tessellationShader;
                bool const supportsLogicalBlendOps    = vkPhysicalDeviceFeatures.logicOp;
                bool const supportsNonSolidDrawing    = vkPhysicalDeviceFeatures.fillModeNonSolid;
                bool const supportsAnisotropicSampler = vkPhysicalDeviceFeatures.samplerAnisotropy;
                bool const supportsBlockCompressedFmt = vkPhysicalDeviceFeatures.textureCompressionBC;

                bool const allFeaturesSupported = (
                            // isDiscreteGPU              &&
                            supportsGeometryShaders    &&
                            supportsTesselationShaders &&
                            supportsLogicalBlendOps    &&
                            supportsNonSolidDrawing    &&
                            supportsAnisotropicSampler &&
                            supportsBlockCompressedFmt);

                if(not allFeaturesSupported)
                {
                    continue;
                }

                //
                // Check Extension support
                //
                std::vector<VkExtensionProperties> supportedExtensions     = {};
                uint32_t                           supportedExtensionCount = 0;

                result = vkEnumerateDeviceExtensionProperties(vkPhysicalDevice, nullptr, &supportedExtensionCount, nullptr);
                if(VkResult::VK_SUCCESS == result)
                {
                    supportedExtensions.resize(supportedExtensionCount);
                    vkEnumerateDeviceExtensionProperties(vkPhysicalDevice, nullptr, &supportedExtensionCount, supportedExtensions.data());
                }

                std::set<std::string> extensions(requiredExtensions.begin(), requiredExtensions.end());

                for(VkExtensionProperties const&extension : supportedExtensions)
                {
                    extensions.erase(extension.extensionName);
                }

                if(!extensions.empty())
                {
                    // Not all extensions supported, skip
                    continue;
                }

                //
                // Check Queue Families for support
                //
                std::vector<VkQueueFamilyProperties> vkQueueFamilies{};

                uint32_t vkQueueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &vkQueueFamilyCount, nullptr);
                vkQueueFamilies.resize(vkQueueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &vkQueueFamilyCount, vkQueueFamilies.data());

                SVulkanQueueFamilyRegistry supportingQueueFamilies{};

                for(uint32_t j=0; j<vkQueueFamilyCount; ++j)
                {
                    VkQueueFamilyProperties const &properties = vkQueueFamilies.at(j);

                    bool enoughQueues             = (properties.queueCount > 0);
                    bool supportsGraphics         = (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT);
                    bool supportsCompute          = (properties.queueFlags & VK_QUEUE_COMPUTE_BIT);
                    bool supportsTransfer         = (properties.queueFlags & VK_QUEUE_TRANSFER_BIT);
                    bool isExclusiveComputeQueue  = (properties.queueFlags == VK_QUEUE_TRANSFER_BIT);
                    bool isExclusiveTransferQueue = (properties.queueFlags == VK_QUEUE_TRANSFER_BIT);

                    // TODO: Perform check for transfer only queue family, so that we can speed up transfer ops!

                    bool queueValid = (
                                enoughQueues     &&
                                supportsGraphics &&
                                supportsCompute);

                    if(!queueValid)
                    {
                        continue;
                    }

                    // Store queue family in common lists and specific lists
                    supportingQueueFamilies.supportingQueueFamilyIndices.push_back(j);
                    if(supportsGraphics)
                        supportingQueueFamilies.graphicsQueueFamilyIndices.push_back(j);
                    if(supportsCompute)
                        supportingQueueFamilies.computeQueueFamilyIndices.push_back(j);
                    if(supportsTransfer)
                        supportingQueueFamilies.transferQueueFamilyIndices.push_back(j);
                    if(isExclusiveComputeQueue)
                    {
                        supportingQueueFamilies.exclusiveComputeQueueFamilyIndices.push_back(j);
                        supportingQueueFamilies.computeQueueFamilyIndices.erase(
                            std::remove_if( supportingQueueFamilies.computeQueueFamilyIndices.begin()
                                          , supportingQueueFamilies.computeQueueFamilyIndices.end()
                                          , [j] (uint32_t const &aIndex) -> bool { return (j == aIndex); }
                            )
                            , supportingQueueFamilies.computeQueueFamilyIndices.end()
                        );
                    }
                    if(isExclusiveTransferQueue)
                    {
                        supportingQueueFamilies.exclusiveTransferQueueFamilyIndices.push_back(j);
                        supportingQueueFamilies.graphicsQueueFamilyIndices.erase(
                            std::remove_if( supportingQueueFamilies.graphicsQueueFamilyIndices.begin()
                                          , supportingQueueFamilies.graphicsQueueFamilyIndices.end()
                                          , [j] (uint32_t const &aIndex) -> bool { return (j == aIndex); }
                            )
                            , supportingQueueFamilies.graphicsQueueFamilyIndices.end()
                        );
                    }
                }

                if(supportingQueueFamilies.supportingQueueFamilyIndices.empty())
                {
                    // Physical device provides no single supporting queue family
                    continue;
                }

                //
                // Check Memory support
                //
                VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties{};
                vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &vkPhysicalDeviceMemoryProperties);

                //
                // Create and store collection of information for this device
                //
                SVulkanPhysicalDevice physicalDevice = {};
                physicalDevice.handle           = vkPhysicalDevice;
                physicalDevice.properties       = vkPhysicalDeviceProperties;
                physicalDevice.features         = vkPhysicalDeviceFeatures;
                physicalDevice.memoryProperties = vkPhysicalDeviceMemoryProperties;
                physicalDevice.queueFamilies    = supportingQueueFamilies;

                supportedPhysicalDevices.push_back(physicalDevice);
            }
        }

        if(supportedPhysicalDevices.empty())
        {
            throw CVulkanError("No supporting physical devices found.", VkResult::VK_SUBOPTIMAL_KHR);
        }

        mVkState.deviceLayers             = requiredLayers;
        mVkState.deviceExtensions         = requiredExtensions;
        mVkState.supportedPhysicalDevices = supportedPhysicalDevices;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::selectPhysicalDevice(uint32_t const &aIndex)
    {
        mVkState.selectedPhysicalDevice = aIndex;

        SVulkanPhysicalDevice const &physicalDevice = mVkState.supportedPhysicalDevices.at(mVkState.selectedPhysicalDevice);

        std::vector<float> queuePriorities{};
        queuePriorities.push_back(1.0f);

        VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo{};
        vkDeviceQueueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        vkDeviceQueueCreateInfo.queueFamilyIndex = physicalDevice.queueFamilies.graphicsQueueFamilyIndices.at(0);
        vkDeviceQueueCreateInfo.queueCount       = queuePriorities.size();
        vkDeviceQueueCreateInfo.pQueuePriorities = queuePriorities.data();
        vkDeviceQueueCreateInfo.flags            = 0;
        vkDeviceQueueCreateInfo.pNext            = nullptr;

        VkDeviceCreateInfo vkDeviceCreateInfo{};
        vkDeviceCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        vkDeviceCreateInfo.enabledLayerCount       = mVkState.deviceLayers.size();
        vkDeviceCreateInfo.ppEnabledLayerNames     = mVkState.deviceLayers.data();
        vkDeviceCreateInfo.enabledExtensionCount   = mVkState.deviceExtensions.size();
        vkDeviceCreateInfo.ppEnabledExtensionNames = mVkState.deviceExtensions.data();
        vkDeviceCreateInfo.pEnabledFeatures        = &(physicalDevice.features);
        vkDeviceCreateInfo.pQueueCreateInfos       = &vkDeviceQueueCreateInfo;
        vkDeviceCreateInfo.queueCreateInfoCount    = 1;
        vkDeviceCreateInfo.flags                   = 0;
        vkDeviceCreateInfo.pNext                   = nullptr;

        VkDevice vkLogicalDevice = VK_NULL_HANDLE;
        VkResult result = vkCreateDevice(physicalDevice.handle, &vkDeviceCreateInfo, nullptr, &vkLogicalDevice);
        if(VkResult::VK_SUCCESS != result)
        {
            throw CVulkanError(CString::format("Failed to create logical device for physical device at index {}.", aIndex), result);
        }

        vkGetPhysicalDeviceProperties(physicalDevice.handle, &(mVkState.properties));

        mVkState.selectedLogicalDevice = vkLogicalDevice;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::createSwapChain(
            math::CRect     const &aRequestedBackBufferSize,
            VkFormat        const &aRequestedFormat,
            VkColorSpaceKHR const &aColorSpace)
    {
        SVulkanPhysicalDevice       &vkPhysicalDevice = mVkState.supportedPhysicalDevices.at(mVkState.selectedPhysicalDevice);
        VkSurfaceKHR          const &vkSurface        = mVkState.surface;

        mVkState.swapChain.requestedBackBufferSize = aRequestedBackBufferSize;
        mVkState.swapChain.requestedFormat         = aRequestedFormat;
        mVkState.swapChain.colorSpace              = aColorSpace;

        //
        // Extract capabilities
        //
        VkSurfaceCapabilitiesKHR vkSurfaceCapabilities = {};

        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice.handle, vkSurface, &vkSurfaceCapabilities);
        if(VkResult::VK_SUCCESS != result)
        {
            throw CVulkanError("Failed to get surface capabilities for physical device and surface.", result);
        }

        //
        // Determine backbuffer extents
        //
        VkExtent2D vkBackBufferExtents = {};
        // Test, whether either the width or height in currentExtent were set to uint32_t::max.
        // In this case, we can differ from the window size and enforce our requested backbuffer size.
        if(vkSurfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            vkBackBufferExtents = vkSurfaceCapabilities.currentExtent;
        }
        else
        {
            bool const requestedBackBufferSizeSupported =
                    (vkSurfaceCapabilities.minImageExtent.width  <= aRequestedBackBufferSize.size.x()) &&
                    (vkSurfaceCapabilities.minImageExtent.height <= aRequestedBackBufferSize.size.y()) &&
                    (vkSurfaceCapabilities.maxImageExtent.width  >= aRequestedBackBufferSize.size.x()) &&
                    (vkSurfaceCapabilities.maxImageExtent.height >= aRequestedBackBufferSize.size.y());
            if(!requestedBackBufferSizeSupported)
            {
                CLog::Warning(
                            logTag(),
                            CString::format(
                                "Requested backbuffer extents {} x {} unsupported. Clamping the extents.",
                                aRequestedBackBufferSize.size.x(),
                                aRequestedBackBufferSize.size.y()));
            }

            VkExtent2D actualExtent = {
                aRequestedBackBufferSize.size.x(),
                aRequestedBackBufferSize.size.y()
            };

            actualExtent.width  = std::max(vkSurfaceCapabilities.minImageExtent.width,  std::min(vkSurfaceCapabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(vkSurfaceCapabilities.minImageExtent.height, std::min(vkSurfaceCapabilities.maxImageExtent.height, actualExtent.height));

            vkBackBufferExtents = actualExtent;
        }

        //
        // Extract Formats
        //
        uint32_t                        vkSurfaceFormatCount = 0;
        std::vector<VkSurfaceFormatKHR> vkSurfaceFormats     = {};

        result = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice.handle, vkSurface, &vkSurfaceFormatCount, nullptr);
        if(VkResult::VK_SUCCESS != result)
        {
            throw CVulkanError("Failed to query the supported surface formats.", result);
        }

        if(vkSurfaceFormatCount == 0)
        {
            throw CVulkanError("No supported surface formats.", VkResult::VK_ERROR_INITIALIZATION_FAILED);
        }

        vkSurfaceFormats.resize(vkSurfaceFormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice.handle, vkSurface, &vkSurfaceFormatCount, vkSurfaceFormats.data());

        //
        // Extract PresentModes
        //
        uint32_t                      vkSurfacePresentModeCount = 0;
        std::vector<VkPresentModeKHR> vkSurfacePresentModes     = {};

        result = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice.handle, vkSurface, &vkSurfacePresentModeCount, nullptr);
        if(VkResult::VK_SUCCESS != result)
        {
            throw CVulkanError("Failed to query the supported surface formats.", result);
        }

        if(vkSurfacePresentModeCount == 0)
        {
            throw CVulkanError("No supported surface present modes.", VkResult::VK_ERROR_INITIALIZATION_FAILED);
        }

        vkSurfacePresentModes.resize(vkSurfacePresentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice.handle, vkSurface, &vkSurfacePresentModeCount, vkSurfacePresentModes.data());

        //
        // Determine format, color space
        //
        VkSurfaceFormatKHR vkSelectedFormat = {};

        if(vkSurfaceFormats.size() == 1 && VK_FORMAT_UNDEFINED == vkSurfaceFormats.at(0).format)
        {
            // We can use any format desired.
            vkSelectedFormat = { aRequestedFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        }
        else
        {
            bool found = false;

            for(VkSurfaceFormatKHR const &surfaceFormat : vkSurfaceFormats)
            {
                found = (surfaceFormat.format == aRequestedFormat);
                if(found)
                {
                    vkSelectedFormat = surfaceFormat;
                    break;
                }
            }

            // No immediately suitable format found. Take the first available.
            if(!found)
            {
                vkSelectedFormat = vkSurfaceFormats.at(0);
            }
        }

        //
        // Determine present mode
        //
        VkPresentModeKHR vkSelectedPresentMode = VK_PRESENT_MODE_FIFO_KHR; // Guaranteed to be available.
        for(VkPresentModeKHR const &presentMode : vkSurfacePresentModes)
        {
            // Try to find a mailbox present mode, as it will allow us to implement triple buffering
            if(VK_PRESENT_MODE_MAILBOX_KHR == presentMode)
            {
                vkSelectedPresentMode = presentMode;
                break;
            }
            // Although Khronos recommends to fall back to immediate mode, let's try FIFO.
            // Maybe the drivers support it properly by now. Nobody wants visible tearing...
            //   else {
            //     vkSelectedPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            //   }
        }

        //
        // Create the swap chain
        //
        VkSwapchainKHR vkSwapChain = VK_NULL_HANDLE;

        // Should give us triple buffering with fallback to double buffering...
        uint32_t swapChainImageCount = (vkSurfaceCapabilities.minImageCount + 1);
        if(vkSurfaceCapabilities.maxImageCount > 0)
        {
            // We have a limited amount of images possible. Clamp!
            swapChainImageCount = std::min(vkSurfaceCapabilities.maxImageCount, swapChainImageCount);
        }

        VkSwapchainCreateInfoKHR vkSwapChainCreateInfo = {};
        vkSwapChainCreateInfo.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        vkSwapChainCreateInfo.surface               = mVkState.surface;
        vkSwapChainCreateInfo.minImageCount         = swapChainImageCount;
        vkSwapChainCreateInfo.imageFormat           = vkSelectedFormat.format;
        vkSwapChainCreateInfo.imageColorSpace       = vkSelectedFormat.colorSpace;
        vkSwapChainCreateInfo.imageExtent           = vkBackBufferExtents;
        vkSwapChainCreateInfo.imageArrayLayers      = 1;
        vkSwapChainCreateInfo.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // Image will also be moved into...
        vkSwapChainCreateInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        vkSwapChainCreateInfo.queueFamilyIndexCount = 0; // Optional due to sharing mode exclusive!
        vkSwapChainCreateInfo.pQueueFamilyIndices   = nullptr;
        vkSwapChainCreateInfo.preTransform          = vkSurfaceCapabilities.currentTransform;
        vkSwapChainCreateInfo.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        vkSwapChainCreateInfo.presentMode           = vkSelectedPresentMode;
        vkSwapChainCreateInfo.flags                 = 0;
        vkSwapChainCreateInfo.clipped               = VK_TRUE;
        vkSwapChainCreateInfo.oldSwapchain          = mVkState.swapChain.handle;
        vkSwapChainCreateInfo.pNext                 = nullptr;

        SVulkanQueueFamilyRegistry const &queueFamilies = vkPhysicalDevice.queueFamilies;

        std::vector<uint32_t> supportedGraphicsQueueFamilyIndices{};

        VkBool32 supported = VK_FALSE;
        for(std::size_t k=0; k<queueFamilies.graphicsQueueFamilyIndices.size(); ++k)
        {
            uint32_t const index   = queueFamilies.graphicsQueueFamilyIndices.at(k);
            VkResult const result2 = vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice.handle, index, vkSurface, &supported);
            if(VkResult::VK_SUCCESS != result2)
            {
                throw CVulkanError("Failed to check for surface support.", result2);
            }

            if(VK_TRUE == supported)
            {
                supportedGraphicsQueueFamilyIndices.push_back(index);
                supported = VK_FALSE;
            }
        }

        if(supportedGraphicsQueueFamilyIndices.empty())
        {
            throw CVulkanError("No supported queue family indices found which support the swapchain on the given surface.", VkResult::VK_ERROR_INITIALIZATION_FAILED);
        }

        result = vkCreateSwapchainKHR(mVkState.selectedLogicalDevice, &vkSwapChainCreateInfo, nullptr, &vkSwapChain);
        if(VkResult::VK_SUCCESS != result)
        {
            throw CVulkanError("Failed to create swapchain.", result);
        }

        //
        // Extract SwapChain images
        //
        uint32_t             createdSwapChainImageCount = 0;
        std::vector<VkImage> swapChainImages            = {};

        result = vkGetSwapchainImagesKHR(mVkState.selectedLogicalDevice, vkSwapChain, &createdSwapChainImageCount, nullptr);
        if(VkResult::VK_SUCCESS != result)
        {
            throw CVulkanError("Failed to fetch swapchain image handles.", result);
        }

        swapChainImages.resize(createdSwapChainImageCount);
        vkGetSwapchainImagesKHR(mVkState.selectedLogicalDevice, vkSwapChain, &createdSwapChainImageCount, swapChainImages.data());

        //
        // PRESENT QUEUES
        //
        std::vector<uint32_t> const &supportingQueueFamilyIndices = vkPhysicalDevice.queueFamilies.supportingQueueFamilyIndices;
        for(std::size_t j=0; j<supportingQueueFamilyIndices.size(); ++j)
        {
            uint32_t const &familyIndex = supportingQueueFamilyIndices.at(j);

            VkBool32 isPresentingSupported = VK_FALSE;

            VkResult const result2 = vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice.handle, familyIndex, vkSurface, &isPresentingSupported);
            if(VK_SUCCESS != result2)
            {
                continue;
            }

            if(isPresentingSupported)
            {
                vkPhysicalDevice.queueFamilies.presentQueueFamilyIndices.push_back(familyIndex);
            }
        }

        //
        // Apply to state
        //
        SVulkanSwapChain swapChain = {};
        swapChain.capabilities             = vkSurfaceCapabilities;
        swapChain.supportedFormats         = vkSurfaceFormats;
        swapChain.supportedPresentModes    = vkSurfacePresentModes;
        swapChain.selectedExtents          = vkBackBufferExtents;
        swapChain.selectedFormat           = vkSelectedFormat;
        swapChain.selectedPresentMode      = vkSelectedPresentMode;
        swapChain.swapChainImages          = swapChainImages;
        swapChain.handle                   = vkSwapChain;

        mVkState.swapChain = swapChain;

        // Now also recreate the command buffers, one foreach swap chain to ensure proper async rendering
        // recreateCommandBuffers(createdSwapChainImageCount); // TBDiscussed: How to permit compute and graphics pipelines in parallel.
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::recreateSwapChain()
    {
        SVulkanState &vkState = getState();

        vkDeviceWaitIdle(vkState.selectedLogicalDevice);

        destroySwapChain();

        math::CRect     const &backBufferSize   = mVkState.swapChain.requestedBackBufferSize;
        VkFormat        const &backBufferFormat = mVkState.swapChain.requestedFormat;
        VkColorSpaceKHR const &colorSpace       = mVkState.swapChain.colorSpace;

        createSwapChain(backBufferSize, backBufferFormat, colorSpace);
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::createCommandPools()
    {
        SVulkanState          &vkState        = getState();
        SVulkanPhysicalDevice &physicalDevice = vkState.supportedPhysicalDevices[vkState.selectedPhysicalDevice];

        std::vector<uint32_t> graphicsQueueIndices = physicalDevice.queueFamilies.graphicsQueueFamilyIndices;
        std::vector<uint32_t> transferQueueIndices = physicalDevice.queueFamilies.exclusiveTransferQueueFamilyIndices;
        if(transferQueueIndices.empty())
        {
            transferQueueIndices = physicalDevice.queueFamilies.transferQueueFamilyIndices;
        }

        auto const createCommandPool = [&] (std::vector<uint32_t> const &queueIndices) -> VkCommandPool
        {
            VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {};
            vkCommandPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            vkCommandPoolCreateInfo.pNext            = nullptr;
            vkCommandPoolCreateInfo.queueFamilyIndex = queueIndices.at(0);
            vkCommandPoolCreateInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            VkCommandPool vkCommandPool = VK_NULL_HANDLE;

            VkResult result = vkCreateCommandPool(vkState.selectedLogicalDevice, &vkCommandPoolCreateInfo, nullptr, &vkCommandPool);
            if(VK_SUCCESS != result)
            {
                throw CVulkanError("Cannot create command pool.", result);
            }

            return vkCommandPool;
        };

        vkState.commandPools.resize(sAspectCount);
        vkState.commandPools[sTransferAspectIndex] = createCommandPool(transferQueueIndices);
        vkState.commandPools[sGraphicsAspectIndex] = createCommandPool(graphicsQueueIndices);
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::destroyCommandPools()
    {
        for(VkCommandPool &pool : getState().commandPools)
        {
            vkDestroyCommandPool(getLogicalDevice(), pool, nullptr);
        }
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::recreateCommandBuffers(uint32_t const &aBufferCount)
    {
        SVulkanState &vkState = getState();

        destroyCommandBuffers();

        //
        // For each swapchain image there'll be a graphics command buffer and a transfer command buffer,
        // submitted in order: transfer -> graphics
        //
        vkState.commandBuffers.resize(sAspectCount);
        vkState.commandBuffers[sTransferAspectIndex].resize(aBufferCount);
        vkState.commandBuffers[sGraphicsAspectIndex].resize(aBufferCount);

        auto const createCommandBuffers = [&] (uint32_t const &aIndex) -> void
        {
            VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo = {};
            vkCommandBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            vkCommandBufferAllocateInfo.pNext              = nullptr;
            vkCommandBufferAllocateInfo.commandPool        = vkState.commandPools[aIndex];
            vkCommandBufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            vkCommandBufferAllocateInfo.commandBufferCount = aBufferCount;

            VkResult const result = vkAllocateCommandBuffers(vkState.selectedLogicalDevice, &vkCommandBufferAllocateInfo, vkState.commandBuffers[aIndex].data());
            if (VkResult::VK_SUCCESS != result)
            {
                throw CVulkanError("Cannot create command buffer(s).", result);
            }
        };

        createCommandBuffers(sTransferAspectIndex);
        createCommandBuffers(sGraphicsAspectIndex);
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::destroyCommandBuffers()
    {
        SVulkanState &vkState = getState();

        auto const destroyBuffers = [&] (uint32_t const &aAspect) -> void
        {
            // Cleanup the old command buffers, if any.
            bool const hasCommandBuffers = (not vkState.commandBuffers.empty() && not vkState.commandBuffers.at(aAspect).empty());
            if(hasCommandBuffers)
            {
                vkFreeCommandBuffers(
                            vkState.selectedLogicalDevice,
                            vkState.commandPools[aAspect],
                            static_cast<uint32_t>(vkState.commandBuffers.at(aAspect).size()),
                            vkState.commandBuffers.at(aAspect).data());

                vkState.commandBuffers.at(aAspect).clear();
            }
        };

        destroyBuffers(sGraphicsAspectIndex);
        destroyBuffers(sTransferAspectIndex);
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::recreateSemaphores(uint32_t const &aBufferCount)
    {
        SVulkanState &state = getState();
        state.imageAvailableSemaphores   .resize(aBufferCount);
        state.transferCompletedSemaphores.resize(aBufferCount);
        state.renderCompletedSemaphores  .resize(aBufferCount);

        for(uint32_t k=0; k<aBufferCount; ++k)
        {
            //
            // Finally: Create an image availability and render completed semaphore to access the swapchain images for rendering operations.
            //
            VkSemaphore vkImageAvailableSemaphore    = VK_NULL_HANDLE;
            VkSemaphore vkTransferCompletedSemaphore = VK_NULL_HANDLE;
            VkSemaphore vkRenderCompletedSemaphore   = VK_NULL_HANDLE;

            VkSemaphoreCreateInfo vkImageAvailableSemaphoreCreateInfo{};
            vkImageAvailableSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            vkImageAvailableSemaphoreCreateInfo.pNext = nullptr;
            vkImageAvailableSemaphoreCreateInfo.flags = 0;

            VkResult result = vkCreateSemaphore(mVkState.selectedLogicalDevice, &vkImageAvailableSemaphoreCreateInfo, nullptr, &vkImageAvailableSemaphore);
            if(VkResult::VK_SUCCESS != result)
            {
                throw CVulkanError("Cannot create image availability semaphore for the swapchain.", result);
            }

            VkSemaphoreCreateInfo vkTransferCompletedSemaphoreCreateInfo{};
            vkTransferCompletedSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            vkTransferCompletedSemaphoreCreateInfo.pNext = nullptr;
            vkTransferCompletedSemaphoreCreateInfo.flags = 0;

            result = vkCreateSemaphore(mVkState.selectedLogicalDevice, &vkTransferCompletedSemaphoreCreateInfo, nullptr, &vkTransferCompletedSemaphore);
            if(VkResult::VK_SUCCESS != result)
            {
                throw CVulkanError("Cannot create transfer completed semaphore for the swapchain.", result);
            }

            VkSemaphoreCreateInfo vkRenderCompletedSemaphoreCreateInfo{};
            vkRenderCompletedSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            vkRenderCompletedSemaphoreCreateInfo.pNext = nullptr;
            vkRenderCompletedSemaphoreCreateInfo.flags = 0;

            result = vkCreateSemaphore(mVkState.selectedLogicalDevice, &vkRenderCompletedSemaphoreCreateInfo, nullptr, &vkRenderCompletedSemaphore);
            if(VkResult::VK_SUCCESS != result)
            {
                throw CVulkanError("Cannot create render completed semaphore for the swapchain.", result);
            }

            state.imageAvailableSemaphores   [k] = vkImageAvailableSemaphore;
            state.transferCompletedSemaphores[k] = vkTransferCompletedSemaphore;
            state.renderCompletedSemaphores  [k] = vkRenderCompletedSemaphore;
        }
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::destroySemaphores()
    {
        SVulkanState &state = getState();

        for(VkSemaphore s : state.renderCompletedSemaphores)
        {
            vkDestroySemaphore(getLogicalDevice(), s, nullptr);
        }
        for(VkSemaphore s : state.transferCompletedSemaphores)
        {
            vkDestroySemaphore(getLogicalDevice(), s, nullptr);
        }
        for(VkSemaphore s : state.imageAvailableSemaphores)
        {
            vkDestroySemaphore(getLogicalDevice(), s, nullptr);
        }
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::destroySwapChain()
    {
        SVulkanState &vkState = getState();

        VkDevice         &logicalDevice = vkState.selectedLogicalDevice;
        SVulkanSwapChain &swapChain     = vkState.swapChain;

        vkDestroySwapchainKHR(logicalDevice, swapChain.handle, nullptr);

        swapChain.capabilities             = {};
        swapChain.supportedFormats         = {};
        swapChain.supportedPresentModes    = {};
        swapChain.selectedExtents          = {};
        swapChain.selectedFormat           = {};
        swapChain.selectedPresentMode      = {};
        swapChain.swapChainImages          = {};
        swapChain.handle                   = VK_NULL_HANDLE;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    EEngineStatus CVulkanEnvironment::initialize(SApplicationEnvironment        const &aApplicationEnvironment
                                               , Shared<CGpuApiResourceStorage>        aStorage)
    {
        SHIRABE_UNUSED(aApplicationEnvironment);

        try {
            mResourceStorage = std::move(aStorage);

            EEngineStatus status = EEngineStatus::Ok;
            createVulkanInstance("ShirabeEngine Demo");
            determinePhysicalDevices();
            selectPhysicalDevice(0);

            return status;
        }
        catch(CVulkanError const&ve)
        {
            CLog::Error(logTag(), CString::format("CVulkanError (VkResult: {}):\n{}", ve.vulkanResult(), ve.what()));
            return EEngineStatus::Error;
        }
        catch(std::exception const &stde)
        {
            CLog::Error(logTag(), stde.what());
            return EEngineStatus::Error;
        }
        catch(...)
        {
            CLog::Error(logTag(), "Unknown error occurred.");
            return EEngineStatus::Error;
        }
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::initializeRecordingAndSubmission()
    {
        createCommandPools();
        recreateCommandBuffers(mVkState.swapChain.swapChainImages.size());
        recreateSemaphores(mVkState.swapChain.swapChainImages.size());
    }
    //<-----------------------------------------------------------------------------

    //<-------------------------------------EEngineStatus----------------------------------------
    //
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::deinitializeRecordingAndSubmission()
    {
        destroySemaphores();
        destroyCommandBuffers();
        destroyCommandPools();
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    EEngineStatus CVulkanEnvironment::deinitialize()
    {
        // Wait for the logical device to finish up all work.
        vkDeviceWaitIdle(mVkState.selectedLogicalDevice);

        // Destroy command buffers and pool

        deinitializeRecordingAndSubmission();

        // Swapchain
        destroySwapChain();

        // Kill it with fire...
        vkDestroyDevice(mVkState.selectedLogicalDevice, nullptr);

#ifdef SHIRABE_DEBUG
        // Remember to destroy the debug report callback...
        PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
                (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(mVkState.instance, "vkDestroyDebugReportCallbackEXT");

        if(!vkDestroyDebugReportCallbackEXT)
        {
            CLog::Warning(logTag(), "Failed to fetch vulkan extension function 'vkDestroyDebugReportCallbackEXT'");
        }
        else
        {
            vkDestroyDebugReportCallbackEXT(mVkState.instance, mVkState.debugReportCallback, nullptr);
        }
#endif

        vkDestroyInstance(mVkState.instance, nullptr);

        return EEngineStatus::Ok;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    VkQueue CVulkanEnvironment::getTransferQueue()
    {
        SVulkanPhysicalDevice const&physicalDevice = mVkState.supportedPhysicalDevices.at(mVkState.selectedPhysicalDevice);

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(getLogicalDevice(), physicalDevice.queueFamilies.transferQueueFamilyIndices.at(0), 0, &queue);

        return queue;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    VkQueue CVulkanEnvironment::getGraphicsQueue()
    {
        SVulkanPhysicalDevice const&physicalDevice = mVkState.supportedPhysicalDevices.at(mVkState.selectedPhysicalDevice);

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(getLogicalDevice(), physicalDevice.queueFamilies.graphicsQueueFamilyIndices.at(0), 0, &queue);

        return queue;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    VkQueue CVulkanEnvironment::getPresentQueue()
    {
        SVulkanPhysicalDevice const&physicalDevice = mVkState.supportedPhysicalDevices.at(mVkState.selectedPhysicalDevice);

        VkQueue queue = VK_NULL_HANDLE;
        if(physicalDevice.queueFamilies.presentQueueFamilyIndices.empty())
        {
            CLog::Error(logTag(), "No present queues available.");
        }
        else
        {
            vkGetDeviceQueue(getLogicalDevice(), physicalDevice.queueFamilies.presentQueueFamilyIndices.at(0), 0, &queue);
        }

        return queue;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    void CVulkanEnvironment::setSurface(VkSurfaceKHR const &aSurface)
    {
        SVulkanState &vkState = getState();

        vkState.surface = aSurface;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //<
    //<-----------------------------------------------------------------------------
    SVulkanState &CVulkanEnvironment::getState()
    {
        return mVkState;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    EEngineStatus CVulkanEnvironment::bindSwapChain()
    {
        SVulkanState &vkState = getState();

        uint32_t nextImageIndex = 0;

        VkDevice       device    = vkState.selectedLogicalDevice;
        VkSwapchainKHR swapChain = vkState.swapChain.handle;
        VkSemaphore    semaphore = vkState.imageAvailableSemaphores.back();
        uint64_t const timeout   =  std::numeric_limits<uint64_t>::max();

        VkResult result = VK_SUCCESS;

        do
        {
            result =
                    vkAcquireNextImageKHR(
                            device,
                            swapChain,
                            timeout,
                            semaphore,
                            VK_NULL_HANDLE,
                            &nextImageIndex);

            switch(result)
            {
                case VkResult::VK_ERROR_OUT_OF_DATE_KHR:
                case VkResult::VK_SUBOPTIMAL_KHR:
                    recreateSwapChain();
                    swapChain = vkState.swapChain.handle;
                    semaphore = vkState.imageAvailableSemaphores.back();
                    break;
                case VkResult::VK_TIMEOUT:
                case VkResult::VK_NOT_READY:
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    break;
                case VkResult::VK_SUCCESS:
                    break;
                default:
                    throw CVulkanError("Failed to execute 'vkAcquireNextImageKHR'", result);
            }

        }
        while(VkResult::VK_SUCCESS != result);

        vkState.swapChain.currentSwapChainImageIndex = nextImageIndex;

        // VkImage      &image    = vkSwapChain.swapChainImages.at(static_cast<uint64_t>(nextImageIndex));
        // Shared<void>  resource = Shared<void>(static_cast<void*>(&image), [] (void*) {});

        return EEngineStatus::Ok;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    CEngineResult<Shared<IVkFrameContext>> CVulkanEnvironment::beginGraphicsFrame()
    {
        if(nullptr != mCurrentFrameContext)
        {
            return { EEngineStatus::Error };
        }

        SVulkanState &state = getState();

        //
        // Move all semaphores to the left, in order to pick the next valid one from the back.
        //
        std::rotate(state.imageAvailableSemaphores   .begin(), state.imageAvailableSemaphores   .begin() + 1, state.imageAvailableSemaphores   .end());
        std::rotate(state.transferCompletedSemaphores.begin(), state.transferCompletedSemaphores.begin() + 1, state.transferCompletedSemaphores.end());
        std::rotate(state.renderCompletedSemaphores  .begin(), state.renderCompletedSemaphores  .begin() + 1, state.renderCompletedSemaphores  .end());

        bindSwapChain(); // Will derive the currentSwapChainImageIndex;


        VkCommandBuffer transferCmdBuffer = state.commandBuffers.at(sTransferAspectIndex).at(state.swapChain.currentSwapChainImageIndex);
        VkCommandBuffer graphicsCmdBuffer = state.commandBuffers.at(sGraphicsAspectIndex).at(state.swapChain.currentSwapChainImageIndex);

        CVulkanFrameContext::SFrameContextData data {};
        data.graphicsCommandBuffer      = graphicsCmdBuffer;
        data.transferCommandBuffer      = transferCmdBuffer;
        data.imageAvailableSemaphore    = state.imageAvailableSemaphores   .back();
        data.transferCompletedSemaphore = state.transferCompletedSemaphores.back();
        data.renderCompletedSemaphore   = state.renderCompletedSemaphores  .back();

        mCurrentFrameContext = makeShared<CVulkanFrameContext>(data);

        return { EEngineStatus::Ok, getVkCurrentFrameContext() };
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    CEngineResult<> CVulkanEnvironment::endGraphicsFrame() {

        mCurrentFrameContext = nullptr;

        return EEngineStatus::Ok;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    Shared<IVkFrameContext> CVulkanEnvironment::getVkCurrentFrameContext()
    {
        return mCurrentFrameContext;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    VkDevice CVulkanEnvironment::getLogicalDevice()
    {
        return getState().selectedLogicalDevice;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    VkPhysicalDevice CVulkanEnvironment::getPhysicalDevice()
    {
        SVulkanState &state = getState();
        return state.supportedPhysicalDevices.at(state.selectedPhysicalDevice).handle;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    Shared<CGpuApiResourceStorage> CVulkanEnvironment::getResourceStorage()
    {
        return mResourceStorage;
    }
    //<-----------------------------------------------------------------------------

}
