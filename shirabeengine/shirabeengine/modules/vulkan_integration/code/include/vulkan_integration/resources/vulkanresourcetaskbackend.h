#ifndef __SHIRABE_RESOURCES_VULKAN_TASKBUILDER_H__
#define __SHIRABE_RESOURCES_VULKAN_TASKBUILDER_H__

#include <map>

#include "vulkan_integration/vulkanimport.h"
#include "vulkan_integration/vulkanenvironment.h"

namespace engine
{
    namespace vulkan
    {
        using namespace engine::resources;
        using namespace engine::gfxapi;

        #define VkTypes 	        \
                   VkSwapchainKHR   \
                   , VkRenderPass   \
                   , VkFrameBuffer  \
                   , VkImage        \
                   , VkBuffer       \
                   , VkImageView    \
                   , VkBufferView   \
                   , VkShaderModule \
                   , VkPipeline     \
                   , Vk
    }

    #define SHIRABE_DECLARE_TASK_BUILDER_MODULE(type)                          \
        CEngineResult<> fn##type##CreationTask(                                \
                            C##type::CCreationRequest      const &aRequest,    \
                            ResolvedDependencyCollection_t const &aDepencies,  \
                            ResourceTaskFn_t                     &aOutTask);   \
        CEngineResult<> fn##type##UpdateTask(                                  \
                            C##type::CUpdateRequest         const &aRequest,   \
                            SGFXAPIResourceHandleAssignment const &aAssignment,\
                            ResolvedDependencyCollection_t  const &aDepencies, \
                            ResourceTaskFn_t                      &aOutTask);  \
        CEngineResult<> fn##type##DestructionTask(                             \
                            C##type::CDestructionRequest    const &aRequest,   \
                            SGFXAPIResourceHandleAssignment const &aAssignment,\
                            ResolvedDependencyCollection_t  const &aDepencies, \
                            ResourceTaskFn_t                      &aOutTask);  \
        CEngineResult<> fn##type##QueryTask(                                   \
                            C##type::CQuery                 const &aRequest,   \
                            SGFXAPIResourceHandleAssignment const &aAssignment,\
                            ResourceTaskFn_t                      &aOutTask);

    namespace vulkan
    {
        /**
         * The CVulkanResourceTaskBackend class implements the graphics API resource task
         * backend for the vulkan API.
         */
        class CVulkanResourceTaskBackend
                : public CGFXAPIResourceTaskBackend
        {
            SHIRABE_DECLARE_LOG_TAG(CVulkanResourceTaskBackend);

        public_constructors:
            /**
             * Construct a vulkan resource task backend from a vulkan environment.
             *
             * @param aVulkanEnvironment The vulkan environment to attach the backend to.
             */
            CVulkanResourceTaskBackend(Shared<CVulkanEnvironment> const &aVulkanEnvironment);

        public_methods:
            /**
             * Initialize the backend.
             */
            CEngineResult<> initialize();

            /**
             * Deinitialize the backend.
             */
            CEngineResult<> deinitialize();

        private_methods:
            SHIRABE_DECLARE_TASK_BUILDER_MODULE(Texture);
            SHIRABE_DECLARE_TASK_BUILDER_MODULE(TextureView);
            SHIRABE_DECLARE_TASK_BUILDER_MODULE(Buffer);
            SHIRABE_DECLARE_TASK_BUILDER_MODULE(BufferView);
            //DeclareTaskBuilderModule(Buffer);
            //DeclareTaskBuilderModule(BufferView);
            SHIRABE_DECLARE_TASK_BUILDER_MODULE(RenderPass);
            SHIRABE_DECLARE_TASK_BUILDER_MODULE(FrameBuffer);
            SHIRABE_DECLARE_TASK_BUILDER_MODULE(Pipeline);
            SHIRABE_DECLARE_TASK_BUILDER_MODULE(Mesh);

        private_members:
            Shared<CVulkanEnvironment> mVulkanEnvironment;
        };


    }
}

#endif
