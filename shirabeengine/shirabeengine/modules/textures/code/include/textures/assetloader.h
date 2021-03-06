//
// Created by dotti on 24.11.19.
//

#ifndef __SHIRABEDEVELOPMENT_TEXTURE_ASSETLOADER_H__
#define __SHIRABEDEVELOPMENT_TEXTURE_ASSETLOADER_H__

#include <asset/assetstorage.h>
#include <resources/resourcedescriptions.h>
#include <resources/resourcetypes.h>
#include <resources/cresourcemanager.h>
#include "textures/declaration.h"
#include "textures/loader.h"

namespace engine::textures
{
    using namespace resources;

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    static Shared<CResourceFromAssetResourceObjectCreator<STexture>> getAssetLoader(Shared<resources::CResourceManager> const &aResourceManager
                                                                                   , Shared<asset::IAssetStorage>       const &aAssetStorage
                                                                                   , Shared<textures::CTextureLoader>   const &aLoader)
    {
        auto const loader = [=] (ResourceId_t const &aResourceId, AssetId_t const &aAssetId) -> Shared<ILogicalResourceObject>
        {
            auto const &[result, instance] = aLoader->loadInstance(aAssetStorage, aAssetId);
            if(CheckEngineError(result))
            {
                return nullptr;
            }

            Shared<CTextureInstance> textureInstance = instance;

            DataSourceAccessor_t initialData = [=]() -> ByteBuffer
            {
                asset::AssetID_t const uid = textureInstance->imageLayersBinaryAssetUid();

                auto const[result, buffer] = aAssetStorage->loadAssetData(uid);
                if( CheckEngineError(result))
                {
                    CLog::Error("DataSourceAccessor_t::Texture", "Failed to load texture asset data. Result: {}", result);
                    return {};
                }

                return buffer;
            };

            STextureDescription textureDescription {};
            textureDescription.name        = instance->name();
            textureDescription.textureInfo = instance->textureInfo();
            textureDescription.cpuGpuUsage = EResourceUsage::CPU_InitConst_GPU_Read;
            textureDescription.gpuBinding  = EBufferBinding::TextureInput;
            textureDescription.gpuBinding.set(EBufferBinding::CopyTarget);

            textureDescription.initialData = { initialData };

            CEngineResult<Shared<ILogicalResourceObject>> textureObject = aResourceManager->useDynamicResource<STexture>(textureDescription.name, textureDescription);
            EngineStatusPrintOnError(textureObject.result(),  "Texture::AssetLoader", "Failed to load texture.");

            return textureObject.data();
        };

        return makeShared<CResourceFromAssetResourceObjectCreator<STexture>>(loader);
    }
}

#endif //__SHIRABEDEVELOPMENT_ASSETLOADER_H__
