#ifndef __SHIRABE_VULKAN_MESH_RESOURCE_H__
#define __SHIRABE_VULKAN_MESH_RESOURCE_H__

#include <vector>

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#include <base/declaration.h>

namespace engine
{
    namespace vulkan
    {
        /**
         * The SVulkanTextureResource struct describes the relevant data to deal
         * with textures inside the vulkan API.
         */
        struct SVulkanMeshResource
        {
        public_members:
            std::vector<VkBuffer>       vertexBuffer;
            std::vector<VkDeviceMemory> vertexBufferMemory;
            VkBuffer                    indexBuffer;
            VkDeviceMemory              indexBufferMemory;
        };
    }
}

#endif
