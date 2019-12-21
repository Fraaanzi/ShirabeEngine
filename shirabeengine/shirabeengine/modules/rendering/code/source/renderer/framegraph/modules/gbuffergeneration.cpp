#include "renderer/framegraph/modules/gbuffergeneration.h"

namespace engine
{
    namespace framegraph
    {
        //<-----------------------------------------------------------------------------
        //
        //<-----------------------------------------------------------------------------
        CEngineResult
        <
            CFrameGraphModule<SGBufferModuleTag_t>::SGBufferGenerationExportData
        >
        CFrameGraphModule<SGBufferModuleTag_t>::addGBufferGenerationPass(
                std::string               const &aPassName,
                CGraphBuilder                   &aGraphBuilder,
                SFrameGraphRenderableList const &aRenderableInput)
        {
            /**
             * The SState struct is the internal state of the gbuffer generation pass.
             */
            struct SState
            {
                SFrameGraphResource gbufferTextureArrayId;
                SFrameGraphResource depthStencilTextureId;
            };

            /**
             * The SPassData struct declares the externally managed pass data
             * for the pass to be created.
             */
            struct SPassData
            {
                SGBufferGenerationImportData importData;
                SGBufferGenerationExportData exportData;

                SState state;
            };

            // ----------------------------------------------------------------------------------
            // Setup
            // ----------------------------------------------------------------------------------
            auto const setup = [&] (
                    CPassBuilder &aBuilder,
                    SPassData    &aOutPassData)
                    -> CEngineResult<>
            {
                // Default extents.
                uint32_t width  = 1920;
                uint32_t height = 1080;

                Shared<SApplicationEnvironment> environment = aGraphBuilder.applicationEnvironment();
                if(environment)
                {
                    Shared<wsi::CWSIDisplay> const &display     = aGraphBuilder.display();
                    SOSDisplayDescriptor     const &displayDesc = display->screenInfo()[display->primaryScreenIndex()];
                    width  = displayDesc.bounds.size.x();
                    height = displayDesc.bounds.size.y();
                }

                SFrameGraphTexture gbufferDesc={ };
                gbufferDesc.width          = width;
                gbufferDesc.height         = height;
                gbufferDesc.depth          = 1;
                gbufferDesc.format         = FrameGraphFormat_t::B8G8R8A8_UNORM;
                gbufferDesc.initialState   = EFrameGraphResourceInitState::Clear;
                gbufferDesc.arraySize      = 3;
                gbufferDesc.mipLevels      = 1;
                gbufferDesc.permittedUsage =   EFrameGraphResourceUsage::InputAttachment
                                             | EFrameGraphResourceUsage::ColorAttachment;

                SFrameGraphTexture depthStencilDesc={ };
                depthStencilDesc.width          = width;
                depthStencilDesc.height         = height;
                depthStencilDesc.depth          = 1;
                depthStencilDesc.format         = FrameGraphFormat_t::D24_UNORM_S8_UINT;
                depthStencilDesc.initialState   = EFrameGraphResourceInitState::Clear;
                depthStencilDesc.arraySize      = 1;
                depthStencilDesc.mipLevels      = 1;
                depthStencilDesc.permittedUsage =   EFrameGraphResourceUsage::InputAttachment
                                                  | EFrameGraphResourceUsage::DepthAttachment;

                // Basic underlying output buffer to be linked
                aOutPassData.state.gbufferTextureArrayId = aBuilder.createTexture("GBuffer Array Texture", gbufferDesc).data();
                aOutPassData.state.depthStencilTextureId = aBuilder.createTexture("DepthStencil Texture", depthStencilDesc).data();

                // This will create a list of render targets for the texutre array to render to.
                // They'll be internally created and managed.
                SFrameGraphWriteTextureFlags baseFlags = {};
                baseFlags.requiredFormat  = gbufferDesc.format;
                baseFlags.writeTarget     = EFrameGraphWriteTarget::Color;
                baseFlags.arraySliceRange = CRange(0, 1);
                baseFlags.mipSliceRange   = CRange(0, 1);

                SFrameGraphWriteTextureFlags write0 = baseFlags,
                                             write1 = baseFlags,
                                             write2 = baseFlags;

                write0.arraySliceRange.offset = 0;
                write1.arraySliceRange.offset = 1;
                write2.arraySliceRange.offset = 2;

                SFrameGraphWriteTextureFlags depthFlags = {};
                depthFlags.requiredFormat  = depthStencilDesc.format;
                depthFlags.writeTarget     = EFrameGraphWriteTarget::Depth;
                depthFlags.arraySliceRange = CRange(0, 1);
                depthFlags.mipSliceRange   = CRange(0, 1);

                aOutPassData.exportData.gbuffer0     = aBuilder.writeAttachment(aOutPassData.state.gbufferTextureArrayId, write0).data();
                aOutPassData.exportData.gbuffer1     = aBuilder.writeAttachment(aOutPassData.state.gbufferTextureArrayId, write1).data();
                aOutPassData.exportData.gbuffer2     = aBuilder.writeAttachment(aOutPassData.state.gbufferTextureArrayId, write2).data();
                aOutPassData.exportData.depthStencil = aBuilder.writeAttachment(aOutPassData.state.depthStencilTextureId, depthFlags).data();

                // Register all meshes and materials for use.
                for(SRenderable const &renderable : aRenderableInput.renderableList)
                {
                    SRenderableResources resources {};
                    if(0 != renderable.meshInstanceAssetId)
                    {
                        SFrameGraphMesh const &meshResource = aBuilder.useMesh(renderable.meshInstanceId,
                                                                               renderable.meshInstanceAssetId).data();
                        resources.meshResource = meshResource;
                    }

                    if(0 != renderable.materialInstanceAssetId)
                    {
                        SFrameGraphMaterial const &materialResource = aBuilder.useMaterial(renderable.materialInstanceId,
                                                                                           renderable.materialInstanceAssetId).data();
                        resources.materialResource = materialResource;
                    }

                    aOutPassData.importData.renderables.push_back(resources);
                }

                return { EEngineStatus::Ok };
            };

            // ----------------------------------------------------------------------------------
            // Execution
            // ----------------------------------------------------------------------------------
            auto const execute = [=] (
                    SPassData                        const&aPassData,
                    CFrameGraphResources             const&aFrameGraphResources,
                    Shared<IFrameGraphRenderContext>      &aRenderContext)
                    -> CEngineResult<>
            {
                using namespace engine::rendering;

                CLog::Verbose(logTag(), "GBufferGeneration");

                // aRenderContext->clearAttachments("DefaultRenderPass");

                for(SRenderableResources const &renderableResources : aPassData.importData.renderables)
                {
                    auto const &[result, materialPointer] = aFrameGraphResources.get<SFrameGraphMaterial>(renderableResources.materialResource.resourceId);
                    if(CheckEngineError(result) || nullptr == materialPointer)
                    {
                        CLog::Error(logTag(), "Failed to fetch material for id {}", renderableResources.materialResource);
                        continue;
                    }

                    auto const &[result2, meshPointer] = aFrameGraphResources.get<SFrameGraphMesh>(renderableResources.meshResource.resourceId);
                    if(CheckEngineError(result2) || nullptr == meshPointer)
                    {
                        CLog::Error(logTag(), "Failed to fetch mesh for id {}", renderableResources.meshResource);
                        continue;
                    }

                    aRenderContext->render(*meshPointer, *materialPointer);
                }

                return { EEngineStatus::Ok };
            };

            // ----------------------------------------------------------------------------------
            // Spawn the pass
            // ----------------------------------------------------------------------------------
            auto passFetch = aGraphBuilder.spawnPass<CallbackPass<SPassData>>(aPassName, setup, execute);
            if(not passFetch.successful())
            {
                return { EEngineStatus::Error };
            }
            else
            {
                Shared<CallbackPass<SPassData>> pass = passFetch.data();
                if(nullptr == pass)
                {
                    return { EEngineStatus::NullPointer };
                }
                else
                {
                    return { EEngineStatus::Ok, passFetch.data()->passData().exportData };
                }
            }
        }
        //<-----------------------------------------------------------------------------
    }
}
