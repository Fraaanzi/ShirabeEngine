#include "ecws/transformcomponent.h"
#include "ecws/entity.h"

namespace engine::ecws
{
    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    CTransformComponent::CTransformComponent(std::string const &aName)
        : CComponentBase(aName)
        //, mMaterialInstance(nullptr)
    {
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    CTransformComponent::~CTransformComponent()
    {
        //mMaterialInstance = nullptr;
    }
    //<-----------------------------------------------------------------------------

    //<-----------------------------------------------------------------------------
    //
    //<-----------------------------------------------------------------------------
    EEngineStatus CTransformComponent::update(CTimer const &aTimer)
    {
        SHIRABE_UNUSED(aTimer);

        return EEngineStatus::Ok;
    }
    //<-----------------------------------------------------------------------------
}
