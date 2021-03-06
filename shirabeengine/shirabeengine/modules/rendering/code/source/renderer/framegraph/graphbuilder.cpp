﻿#include <assert.h>
#include "renderer/framegraph/graphbuilder.h"

namespace engine
{
    namespace framegraph
    {
        /**
         * Function signature used for a check for a uid within a list of UIDs.
         */
        template <typename TUID>
        using AlreadyRegisteredFn_t = std::function<bool(std::vector<TUID> const&, TUID const&)>;

        /**
         * Functor usable as a predicate to determine, whether adjacency info was already stored for a
         * node UID.
         */
        template <typename TUID>
        static AlreadyRegisteredFn_t<TUID> alreadyRegisteredFn =
                [] (std::vector<TUID> const &aAdjacency,
                    TUID              const &aPossiblyAdjacent)
        {
            return (aAdjacency.end() != std::find(aAdjacency.begin(), aAdjacency.end(), aPossiblyAdjacent));
        };

        //<-----------------------------------------------------------------------------
        //
        //<-----------------------------------------------------------------------------
        CGraphBuilder::CGraphBuilder()
            : mGraphMode              (framegraph::CGraph::EGraphMode::Compute                                        )
            , mRenderToBackBuffer     (false                                                                          )
            , mOutputResourceId       (0                                                                              )
            , mApplicationEnvironment (nullptr                                                                        )
            , mDisplay                (nullptr                                                                        )
            , mPassUIDGenerator       (std::make_shared<CSequenceUIDGenerator<FrameGraphResourceId_t >>(0))
            , mResourceUIDGenerator   (std::make_shared<CSequenceUIDGenerator<FrameGraphResourceId_t >>(1))
            , mImportedResources      (                                                                               )
            , mPasses                 (                                                                               )
            , mResources              (                                                                               )
            , mResourceData           (                                                                               )
            , mPassAdjacency          (                                                                               )
            , mFrameGraph             (nullptr                                                                        )
    #if defined SHIRABE_FRAMEGRAPH_ENABLE_SERIALIZATION
            , mResourceAdjacency      (                                          )
            , mPassToResourceAdjacency(                                          )
    #endif
        {}
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        Shared<IUIDGenerator<FrameGraphResourceId_t>> CGraphBuilder::resourceUIDGenerator()
        {
            return mResourceUIDGenerator;
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        FrameGraphResourceId_t CGraphBuilder::generatePassUID()
        {
            return mPassUIDGenerator->generate();
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        Unique<CGraph> &CGraphBuilder::graph()
        {
            return mFrameGraph;
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        Shared<SApplicationEnvironment> &CGraphBuilder::applicationEnvironment()
        {
            return mApplicationEnvironment;
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        Shared<wsi::CWSIDisplay> const &CGraphBuilder::display()
        {
            return mDisplay;
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CEngineResult<> CGraphBuilder::initialize(
                Shared<SApplicationEnvironment> const &aApplicationEnvironment,
                Shared<wsi::CWSIDisplay>        const &aDisplay)
        {
            bool const inputInvalid =
                    nullptr == aApplicationEnvironment or
                    nullptr == aDisplay;

            if(inputInvalid)
            {
                return { EEngineStatus::Error };
            }

            applicationEnvironment() = aApplicationEnvironment;
            mDisplay                 = aDisplay;
            graph()                  = makeUnique<CGraph>();

            // TODO: We add a dummy pass for the algorithm to work... Need to fix that up
            auto const pseudoSetup = [] (CPassBuilder const &, bool &) -> CEngineResult<>
            {
                return { EEngineStatus::Ok };
            };

            auto const pseudoExec  = [] (
                    bool                                      const &,
                    CFrameGraphResources                      const &,
                    Shared<IFrameGraphRenderContext>       &) -> CEngineResult<>
            {
                return { EEngineStatus::Ok };
            };

            // Spawn pseudo pass to simplify algorithms and have "empty" execution blocks.
            spawnPass<CallbackPass<bool>>("Pseudo-Pass", pseudoSetup, pseudoExec);

            return { EEngineStatus::Ok };
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CEngineResult<> CGraphBuilder::deinitialize()
        {
            graph()                  = nullptr;
            applicationEnvironment() = nullptr;

            return { EEngineStatus::Ok };
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CEngineResult<> CGraphBuilder::createPassDependency(
                std::string const &aPassSource,
                std::string const &aPassTarget)
        {
            auto const sourcePredicate = [&] (PassMap::value_type const &aPair) -> bool
            {
                std::string const &name = aPair.second->passName();
                return (0 == aPassSource.compare(name));
            };

            auto const targetPredicate = [&] (PassMap::value_type const &aPair) -> bool
            {
                std::string const &name = aPair.second->passName();
                return (0 == aPassTarget.compare(name));
            };

            PassMap::const_iterator sourcePass = std::find_if(mPasses.begin(), mPasses.end(), sourcePredicate);
            PassMap::const_iterator targetPass = std::find_if(mPasses.begin(), mPasses.end(), targetPredicate);

            bool const containsSourcePass = (mPasses.end() != sourcePass);
            bool const containsTargetPass = (mPasses.end() != targetPass);

            if(not (containsSourcePass && containsTargetPass))
            {
                CLog::Error(logTag(), CString::format("Cannot create pass dependency from {} to {}. At least one pass was not found.", aPassSource, aPassTarget));
                return { EEngineStatus::Error };
            }

            PassUID_t const &sourcePassUID = sourcePass->second->passUID();
            PassUID_t const &targetPassUID = targetPass->second->passUID();

            createPassDependencyByUID(sourcePassUID, targetPassUID);

            return { EEngineStatus::Ok };
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CEngineResult<> CGraphBuilder::createPassDependencyByUID(
                PassUID_t const &aPassSource,
                PassUID_t const &aPassTarget)
        {
            bool const passAlreadyRegistered = alreadyRegisteredFn<PassUID_t>(mPassAdjacency[aPassSource], aPassTarget);
            if(not passAlreadyRegistered)
            {
                mPassAdjacency[aPassSource].push_back(aPassTarget);
            }

            return { passAlreadyRegistered ? EEngineStatus::Error : EEngineStatus::Ok };
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        SFrameGraphResource CGraphBuilder::registerTexture(
                std::string        const &aReadableName,
                SFrameGraphTexture const &aTexture)
        {
            try
            {
                SFrameGraphTexture &resource = mResourceData.spawnResource<SFrameGraphTexture>();
                resource.assignedPassUID    = 0; // Pre-Pass
                resource.parentResource     = 0; // No internal tree, has to be resolved differently.
                resource.type               = EFrameGraphResourceType::Texture;
                resource.readableName       = aReadableName;
                resource.isExternalResource = true;
                resource.assignTextureParameters(aTexture);

                mResources.push_back(resource.resourceId);

                Unique<CPassBase::CMutableAccessor> accessor = mPasses.at(0)->getMutableAccessor(CPassKey<CGraphBuilder>());
                accessor->mutableResourceReferences().push_back(resource.resourceId);

                return resource;
            }
            catch(std::exception)
            {
                CLog::Error(logTag(), CString::format("Failed to register texture {}", aReadableName));
                throw;
            }
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        SFrameGraphRenderableList CGraphBuilder::registerRenderables(
                std::string               const &aReadableIdentifier,
                rendering::RenderableList const &aRenderables)
        {
            try
            {
                SFrameGraphRenderableList &resource = mResourceData.spawnResource<SFrameGraphRenderableList>();
                resource.assignedPassUID    = 0; // Pre-Pass
                resource.parentResource     = 0; // No internal tree, has to be resolved differently.
                resource.type               = EFrameGraphResourceType::RenderableList;
                resource.readableName       = aReadableIdentifier;
                resource.isExternalResource = true;
                resource.renderableList     = aRenderables;

                mResources.push_back(resource.resourceId);

                return resource;
            }
            catch(std::exception)
            {
                CLog::Error(logTag(), CString::format("Failed to register renderable list {}", aReadableIdentifier));
                throw;
            }
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CEngineResult<Unique<CGraph>> CGraphBuilder::compile()
        {
            Unique<CGraph::CMutableAccessor> accessor = graph()->getMutableAccessor(CPassKey<CGraphBuilder>());

            // First (No-Op, automatically performed on call to 'setPassDependency(...)':
            //   Evaluate explicit pass dependencies without resource flow.

            // Second: Evaluate all pass resources and their implicit relations to passes and other resources.
            for(PassMap::value_type const &assignment : graph()->passes())
            {
                CEngineResult<> const collection = collectPass(assignment.second);
                if(not collection.successful())
                {
                    CLog::Error(logTag(), "Failed to perform pass collection.");
                    return { EEngineStatus::Error };
                }
            }

            // Third: Sort the passes by their relationships and dependencies.
            CEngineResult<> const topologicalPassSort = topologicalSort<PassUID_t>(accessor->mutablePassExecutionOrder());
            if(not topologicalPassSort.successful())
            {
                CLog::Error(logTag(), "Failed to perform topologicalSort(...) for passes on graph compilation.");
                return { EEngineStatus::Error };
            }

#if defined SHIRABE_FRAMEGRAPH_ENABLE_SERIALIZATION

            // Fourth: Sort the resources by their relationships and dependencies.
            bool const topologicalResourceSortSuccessful = topologicalSort<FrameGraphResourceId_t>(accessor->mutableResourceOrder());
            if(!topologicalResourceSortSuccessful)
            {
                CLog::Error(logTag(), "Failed to perform topologicalSort(...) for resources on graph compilation.");
                return nullptr;
            }
#endif

#if defined SHIRABE_DEBUG || defined SHIRABE_TEST

            CEngineResult<> const validation = validate(accessor->passExecutionOrder());
            if(not validation.successful())
            {
                CLog::Error(logTag(), "Failed to perform validation(...) on graph compilation.");
                return { EEngineStatus::Error };
            }

#endif

            // Move out the current adjacency state to the frame graph, so that it can be used for further processing.
            // It is no more needed at this point within the GraphBuilder.
            accessor->mutablePassAdjacency() = std::move(this->mPassAdjacency);
            accessor->mutableResources()     = std::move(this->mResources);
            accessor->mutableResourceData().mergeIn(this->mResourceData);

            accessor->mutableGraphMode()               = mGraphMode;
            accessor->mutableRenderToBackBuffer()      = mRenderToBackBuffer;
            accessor->mutableOutputTextureResourceId() = mOutputResourceId;

#if defined SHIRABE_FRAMEGRAPH_ENABLE_SERIALIZATION
            accessor->mutableResourceAdjacency()       = std::move(this->mResourceAdjacency);
            accessor->mutablePassToResourceAdjacency() = std::move(this->mPassToResourceAdjacency);
#endif


            return { EEngineStatus::Ok, std::move(graph()) };
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CEngineResult<FrameGraphResourceId_t> CGraphBuilder::findSubjacentResource(
               SFrameGraphResourceMap const &aResources,
               SFrameGraphResource    const &aStart)
        {
            if(SHIRABE_FRAMEGRAPH_UNDEFINED_RESOURCE == aStart.parentResource)
            {
                return { EEngineStatus::Ok, aStart.resourceId };
            }
            else
            {
                if(aResources.end() != aResources.find(aStart.parentResource))
                {
                    return findSubjacentResource(aResources, aResources.at(aStart.parentResource));
                }
                else
                {
                    return { EEngineStatus::Error };
                }
            }
        }        
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CEngineResult<> CGraphBuilder::collectPass(Shared<CPassBase> aPass)
        {
            if(nullptr == aPass)
            {
                return { EEngineStatus::Error };
            }

            Unique<CPassBase::CMutableAccessor> accessor = aPass->getMutableAccessor(CPassKey<CGraphBuilder>());

            FrameGraphResourceIdList const resources = accessor->mutableResourceReferences();

            for(FrameGraphResourceId_t const &resourceId : resources)
            {
                CEngineResult<Shared<SFrameGraphResource>> resourceFetch = mResourceData.getMutable<SFrameGraphResource>(resourceId);
                if(not resourceFetch.successful())
                {
                    CLog::Error(logTag(), "Could not fetch pass data.");

                    return { resourceFetch.result() };
                }

                SFrameGraphResource &resource = *(resourceFetch.data());

                // For each underlying OR imported resource (textures/buffers or whatever importable)
                if(SHIRABE_FRAMEGRAPH_UNDEFINED_RESOURCE == resource.parentResource)
                {
                    if(EFrameGraphResourceType::Texture == resource.type)
                    {
#if defined SHIRABE_FRAMEGRAPH_ENABLE_SERIALIZATION
                        // Map the resources to it's pass appropriately
                        bool const alreadyRegistered = alreadyRegisteredFn<FrameGraphResourceId_t>(mPassToResourceAdjacency[aPass->passUID()], resource.resourceId);
                        if(!alreadyRegistered)
                        {
                            mPassToResourceAdjacency[aPass->passUID()].push_back(resource.resourceId);
                        }
#endif
                        continue;
                    }
                }
                // For each derived resource (views)
                else
                {
                    // Avoid internal references for passes!
                    // If the edge from pass k to pass k+1 was not added yet.
                    // Create edge: Parent-->Source
                    CEngineResult<Shared<SFrameGraphResource> const> parentResourceFetch = mResourceData.get<SFrameGraphResource>(resource.parentResource);
                    if(not parentResourceFetch.successful())
                    {
                        CLog::Error(logTag(), "Could not fetch pass data.");

                        return { resourceFetch.result() };
                    }

                    SFrameGraphResource const &parentResource = *(parentResourceFetch.data());
                    PassUID_t           const &passUID        = aPass->passUID();

                    // Create a pass dependency used for topologically sorting the graph
                    // to derive the pass execution order.
                    if(parentResource.assignedPassUID != passUID)
                    {
                        createPassDependencyByUID(parentResource.assignedPassUID, passUID);
                    }

#if defined SHIRABE_FRAMEGRAPH_ENABLE_SERIALIZATION

                    // Do the same for the resources!
                    bool const resourceAdjacencyAlreadyRegistered = alreadyRegisteredFn<FrameGraphResourceId_t>(mResourceAdjacency[parentResource.resourceId], resource.resourceId);
                    if(!resourceAdjacencyAlreadyRegistered)
                    {
                        mResourceAdjacency[parentResource.resourceId].push_back(resource.resourceId);
                    }

                    // And map the resources to it's pass appropriately
                    bool const resourceEdgeAlreadyRegistered = alreadyRegisteredFn<FrameGraphResourceId_t>(mPassToResourceAdjacency[passUID], resource.resourceId);
                    if(!resourceEdgeAlreadyRegistered)
                    {
                        mPassToResourceAdjacency[passUID].push_back(resource.resourceId);
                    }

#endif

                    if(EFrameGraphResourceType::TextureView == resource.type)
                    {
                        CEngineResult<Shared<SFrameGraphTexture>>     textureFetch     = mResourceData.getMutable<SFrameGraphTexture>    (resource.subjacentResource);
                        CEngineResult<Shared<SFrameGraphTextureView>> textureViewFetch = mResourceData.getMutable<SFrameGraphTextureView>(resource.resourceId);

                        if(not (textureFetch.successful() and textureViewFetch.successful()))
                        {
                            CLog::Error(logTag(), "Could not fetch texture and/or texture view from resource registry.");
                            return { resourceFetch.result() };
                        }

                        SFrameGraphTexture     &texture     = *(textureFetch.data());
                        SFrameGraphTextureView &textureView = *(textureViewFetch.data());

                        // Auto adjust format if requested
                        if(FrameGraphFormat_t::Automatic == textureView.format)
                        {
                            textureView.format = texture.format;
                        }
                    }
                    else if(EFrameGraphResourceType::BufferView == resource.type)
                    {
                        // TODO
                    }
                    else if(EFrameGraphResourceType::RenderableListView == resource.type)
                    {
                        // TODO
                    }
                }
            }

            // Now that the internal resource references were adjusted and duplicates removed, confirm the index in the graph builder
            for(FrameGraphResourceId_t const &id : accessor->resourceReferences())
            {
                mResources.push_back(id);
            }

#ifdef SHIRABE_DEBUG
            CLog::Verbose(logTag(), CString::format("Current Adjacency State collecting pass '{}':", aPass->passUID()));
            for(auto const &[uid, adjacentUIDs] : mPassAdjacency)
            {
                CLog::Verbose(logTag(), CString::format("  Pass-UID: {}", uid));

                for(PassUID_t const &adjacentUID : adjacentUIDs)
                {
                    CLog::Verbose(logTag(), CString::format("    Adjacent Pass-UID: {}", adjacentUID));
                }
            }
#endif

            return { EEngineStatus::Ok };
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        CEngineResult<> CGraphBuilder::validate(std::stack<PassUID_t> const &aPassOrder)
        {
            SHIRABE_UNUSED(aPassOrder);

            bool allBindingsValid = true;

            for(RefIndex_t::value_type const &textureViewRef : mResourceData.textureViews())
            {
                CEngineResult<Shared<SFrameGraphTextureView> const> textureViewFetch = mResourceData.get<SFrameGraphTextureView>(textureViewRef);
                if(not textureViewFetch.successful())
                {
                    CLog::Error(logTag(), "Failed to get texture view to validate.");
                    return { textureViewFetch.result() };
                }

                SFrameGraphTextureView const &textureView = *(textureViewFetch.data());

                // Adjust resource access flags in the subjacent resource to have the texture creation configure
                // everything appropriately.

                FrameGraphResourceId_t const  subjacentResourceId =  textureView.subjacentResource;

                CEngineResult<Shared<SFrameGraphTexture> const> subjacentFetch = mResourceData.get<SFrameGraphTexture>(subjacentResourceId);
                if(not textureViewFetch.successful())
                {
                    CLog::Error(logTag(), "Failed to get subjacent texture to validate.");
                    return { textureViewFetch.result() };
                }

                SFrameGraphTexture const &texture = *(subjacentFetch.data());

                bool viewBindingValid = true;
                viewBindingValid  = validateTextureView(texture, textureView);
                allBindingsValid &= viewBindingValid;

            } // foreach TextureView

            return { allBindingsValid
                        ? EEngineStatus::Ok
                        : EEngineStatus::Error };
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        bool CGraphBuilder::validateTextureView(
                SFrameGraphTexture     const &aTexture,
                SFrameGraphTextureView const &atextureView)
        {
            bool const usageValid             = validateTextureUsage(aTexture);
            bool const formatValid            = validateTextureFormat(aTexture, atextureView);
            bool const subresourceAccessValid = validateTextureSubresourceAccess(aTexture, atextureView);

            return (usageValid && formatValid && subresourceAccessValid);
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        bool CGraphBuilder::validateTextureUsage(SFrameGraphTexture const &aTexture)
        {
            // Cross both bitsets... permittedUsage should fully contain requestedUsage
            return aTexture.permittedUsage.check(aTexture.requestedUsage);
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        bool CGraphBuilder::validateTextureFormat(
                SFrameGraphTexture     const &aTexture,
                SFrameGraphTextureView const &aTextureView)
        {
            using FormatValue_t = std::underlying_type_t<FrameGraphFormat_t>;

            auto const nearestPowerOf2Ceil = [] (FormatValue_t value) -> uint64_t
            {
                uint64_t power = 2;
                while(value >>= 1)
                {
                    power <<= 1;
                }
                return power;
            };

            FrameGraphFormat_t const &source = aTexture.format;
            FrameGraphFormat_t const &target = aTextureView.format;

            bool formatsCompatible = false;

            switch(target)
            {
            case FrameGraphFormat_t::Undefined:
            case FrameGraphFormat_t::Structured:
                // Invalid!
                formatsCompatible = false;
                break;
            case FrameGraphFormat_t::Automatic:
                // Should never be accessed as automatic is resolved beforehand though...
                break;
            default:
                FormatValue_t sourceUID = static_cast<FormatValue_t>(source);
                FormatValue_t targetUID = static_cast<FormatValue_t>(target);
                // For now -> Simple test: Bitdepth...
                formatsCompatible = (nearestPowerOf2Ceil(sourceUID) == nearestPowerOf2Ceil(targetUID));
                break;
            }

            return formatsCompatible;
        }
        //<-----------------------------------------------------------------------------

        //<-----------------------------------------------------------------------------
        //<
        //<-----------------------------------------------------------------------------
        bool CGraphBuilder::validateTextureSubresourceAccess(
                SFrameGraphTexture     const &aTexture,
                SFrameGraphTextureView const &aTextureView)
        {
            CRange const &arraySliceRange = aTextureView.arraySliceRange;
            CRange const &mipSliceRange   = aTextureView.mipSliceRange;

            bool arraySliceRangeValid = true;
            arraySliceRangeValid &= (arraySliceRange.offset < aTexture.arraySize);
            arraySliceRangeValid &= ((arraySliceRange.offset + static_cast<uint32_t>(arraySliceRange.length)) <= aTexture.arraySize);

            bool mipSliceRangeValid = true;
            mipSliceRangeValid &= (mipSliceRange.offset < aTexture.mipLevels);
            mipSliceRangeValid &= ((mipSliceRange.offset + static_cast<uint32_t>(mipSliceRange.length)) <= aTexture.mipLevels);

            bool const valid = (arraySliceRangeValid && mipSliceRangeValid);
            return valid;
        }
        //<-----------------------------------------------------------------------------
    }
}
