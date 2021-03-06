#ifndef __SHIRABE_IRESOURCEDESCRIPTOR_H__
#define __SHIRABE_IRESOURCEDESCRIPTOR_H__

#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include "core/enginetypehelper.h"
#include "resources/core/eresourcetype.h"

namespace engine
{
    namespace resources
    {
        using PublicResourceId_t       = std::string;
        using PublicResourceIdList_t   = std::vector<PublicResourceId_t>;
        template <typename TValue>
        using PublicResourceIdMapTo_t  = std::unordered_map<PublicResourceId_t, TValue>;
        using SubjacentResourceId_t    = uint64_t;

        /**
         * Resource descriptor base implementation providing information about the
         * resource types, name, and dependencies.
         */
        template <
                EResourceType    TResourceType,
                EResourceSubType TResourceSubtype
                >
        struct SDescriptorImplBase
        {
        public_static_constants:
            static const constexpr EResourceType    sResourceType   =TResourceType;
            static const constexpr EResourceSubType sResourceSubtype=TResourceSubtype;

        public_constructors:

            SHIRABE_INLINE SDescriptorImplBase()
                    : name({})
                      , dependencies({})
            {}

            SHIRABE_INLINE SDescriptorImplBase(SDescriptorImplBase const &aOther)
                : name(aOther.name)
                , dependencies(aOther.dependencies)
            {}

            SHIRABE_INLINE SDescriptorImplBase(SDescriptorImplBase &&aOther)
                : name(std::move(aOther.name))
                , dependencies(std::move(aOther.dependencies))
            {}

        public_methods:
            /**
             * Return the resource type of the described resource.
             *
             * @return See brief.
             */
            EResourceType type() const { return sResourceType; }

            /**
             * Return the resource subtype of the described resource.
             *
             * @return See brief.
             */
            EResourceSubType subtype() const { return sResourceSubtype; }

            /**
             * Return a string representation of this descriptor.
             *
             * @return See brief.
             */
            virtual std::string toString() const
            {
                std::stringstream ss;
                ss
                        << "DescriptorImplementationBase<" << sResourceType << ", " << sResourceSubtype << "> {\n"
                        << "  Name: " << name << "\n"
                        << "  Dependencies: \n";
                for(PublicResourceId_t const&id : dependencies)
                    ss << id << ", \n";

                return ss.str();
            }

        public_members:
            std::string            name;
            PublicResourceIdList_t dependencies;
        };

        // An adapter class will hold the implementation of a DomainTransferObject, e.g.
        // the descriptor_impl_type of ResourceDescriptor<TextureND<1>>.
        // Since the ResourceDescriptor<T> specializiation will derive from the impl. type
        // slicing at this point won't be an issue.
        // The only thing of matter could be static downcasting and public Interface issues
        // with knowing the capabilities of the underlying resource class.

        #define DeclareAdapter(type, name)                                           \
            /**
            *
            */                                                                       \
            template <typename TImplementation>                                      \
            class C##type##Adapter                                                   \
            {								                                         \
            public_typedefs:								                         \
                using type##Implementation_t = TImplementation;                      \
                                                                                     \
            public_constructors:                                                     \
                SHIRABE_INLINE C##type##Adapter(type##Implementation_t const &aName) \
                    : m##name(aName)					 	                         \
                {}											                         \
            public_destructors:                                                      \
                virtual ~C##type##Adapter() = default;                               \
                                                                                     \
                SHIRABE_INLINE const type##Implementation_t name() const             \
                {                                                                    \
                    return static_cast<type##Implementation_t>(m##name);             \
                }                                                                    \
                                                                                     \
            private_members:                                                         \
                type##Implementation_t m##name;                                      \
            };

        DeclareAdapter(ResourceDescriptor,
                       descriptor);

        DeclareAdapter(ResourceBinding,
                       binding);

        DeclareAdapter(ResourceCreationRequest,
                       creationRequest);

        DeclareAdapter(ResourceUpdateRequest,
                       updateRequest);

        DeclareAdapter(ResourceQueryRequest,
                       queryRequest);

        DeclareAdapter(ResourceDestructionRequest,
                       destructionRequest);

        template <typename TResource>
        static inline std::ostream &operator<<(
                std::ostream                      &s,
                typename TResource::Binding const &d)
        {
            return (s << d.toString());
        }

        template <typename TResource>
        static inline std::ostream &operator<<(
                std::ostream                          &s,
                typename TResource::SDescriptor const &d)
        {
            return (s << d.toString());
        }

        template <typename TResource>
        static inline std::ostream &operator<<(
                std::ostream                               &s,
                typename TResource::CCreationRequest const &d)
        {
            return (s << d.toString());
        }

        template <typename TResource>
        static inline std::ostream &operator<<(
                std::ostream                             &s,
                typename TResource::CUpdateRequest const &d)
        {
            return (s << d.toString());
        }

        template <typename TResource>
        static inline std::ostream &operator<<(
                std::ostream                     &s,
                typename TResource::CQuery const &d)
        {
            return (s << d.toString());
        }

        template <typename TResource>
        static inline std::ostream &operator<<(
                std::ostream                                  &s,
                typename TResource::CDestructionRequest const &d)
        {
            return (s << d.toString());
        }
    }
}

#endif
