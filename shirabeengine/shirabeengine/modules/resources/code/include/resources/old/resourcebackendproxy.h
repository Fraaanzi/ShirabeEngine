#ifndef __SHIRABE_RESOURCESUBSYSTEMPROXY_H__
#define __SHIRABE_RESOURCESUBSYSTEMPROXY_H__

#include "core/enginetypehelper.h"
#include "core/enginestatus.h"

#include "resources/core/iresourceproxy.h"
#include "resources/core/resourcedomaintransfer.h"

namespace engine
{
    namespace resources
    {
        /**
         * Proxy-Class proxying a resource backend implementation.
         *
         * @tparam TBackend  The resource backend to proxy.
         * @tparam TResource The resource type proxied.
         */
        template <
                typename TBackend,
                typename TResource
                >
        class CResourceBackendProxy
                : public CGenericProxyBase<TResource>
        {
        public_constructors:
            /**
             * Create a new resource backend proxy from the proxytype, creation
             * request and the respective resource backend.
             *
             * @param proxyType
             * @param resourceBackend
             * @param request
             */
            CResourceBackendProxy(
                    EProxyType                           const &aProxyType,
                    Shared<TBackend>            const &aResourceBackend,
                    typename TResource::CCreationRequest const &aRequest)
                : CGenericProxyBase<TResource>(aProxyType, aRequest)
                , m_backend(aResourceBackend)
            { }

        protected_methods:
            /**
             * Return the pointer to the attached resource backend.
             *
             * @return See brief.
             */
            Shared<TBackend> resourceBackend()
            {
                return m_backend;
            }

        private_members:
            Shared<TBackend> m_backend;
        };
    }
}

#endif
