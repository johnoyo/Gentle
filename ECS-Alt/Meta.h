#pragma once

#include <cstddef>
#include <vector>
#include <unordered_map>

namespace HBL2
{
    namespace Meta
    {
        /*
            We need the following to achieve feature parity with entt:
                - entt::meta_any: to return from the dll of the user
                - entt::meta_ctx: stores the map with the types
                - entt::meta: static API for populating a meta_ctx

            struct NewComponent
            {
                int Mario;
            };

            Meta::Context ctx;

            Meta::Register<NewComponent>(ctx)
                .Data<&NewComponent::Mario>("Mario");

            Meta::TypeData typeData = Meta::Resolve<NewComponent>(ctx).Member("Mario");
            Meta::Any metaType = typeData.Get(componentMetaAny);

            int* num = metaType.TryGetAs<int>();
            bool isInt = metaType.Is<int>();
            typeData.Set(componentMetaAny, 10);

            metaType.assign(componentMeta);
        */
        using TypeID = std::size_t;
        using MemberID = std::size_t;

        // Helpers for generating unique TypeIDs at compile time
        template<typename T>
        struct TypeIndex
        {
            static const TypeID value;
        };
        template<typename T>
        const TypeID TypeIndex<T>::value = reinterpret_cast<TypeID>(&TypeIndex<T>::value);
        
        struct Any;

        // Per‑member information
        struct MemberData
        {
            const char* name;
            std::size_t offset;
            TypeID type;

            // read
            Any Get(Any owner) const;

            // write (POD only)
            void Set(Any owner, Any value) const;

            template<typename T>
            void Set(const Any& owner, const T& v) const;
        };

        // All reflection info for one type
        struct TypeData
        {
            TypeID id;
            const char* name;
            std::size_t size;       // sizeof(T)
            std::size_t alignment;  // alignof(T)
            std::vector<MemberData> members;

            // find member by name
            const MemberData* Member(const char* n) const
            {
                for (auto& m : members)
                {
                    if (strcmp(m.name, n) == 0) return &m;
                }
                return nullptr;
            }
        };

        // The global registry
        struct Context
        {
            std::unordered_map<TypeID, TypeData> types;
        };

        // A type‑erased container holding a pointer to an object of some reflected type
        struct Any
        {
            void* object = nullptr;
            TypeID type = 0;
            Context* ctx = nullptr;

            template<typename T>
            T* TryGetAs()
            {
                if (type == TypeIndex<T>::value)
                {
                    return reinterpret_cast<T*>(object);
                }
                return nullptr;
            }

            template<typename T>
            bool Is() const
            {
                return type == TypeIndex<T>::value;
            }

            void Assign(const Any& other)
            {
                object = other.object;
                type = other.type;
                ctx = other.ctx;
            }

            // Assign from raw void* + TypeID
            void Assign(void* obj, TypeID t, Context* c)
            {
                object = obj;
                type = t;
                ctx = c;
            }

            template<typename T>
            void Assign(T* obj, Context* c)
            {
                object = obj;
                type = TypeIndex<std::remove_const_t<T>>::value;
                ctx = c;

                // auto‑register fundamental or unknown types
                if (!ctx->types.count(type))
                {
                    ctx->types[type] = TypeData{
                        /*id*/        type,
                        /*name*/      typeid(T).name(),
                        /*size*/      sizeof(T),
                        /*alignment*/ alignof(T),
                        /*members*/   {}
                    };
                }
            }            
        };

        inline Any MemberData::Get(Any owner) const {
            assert(owner.ctx);
            void* ptr = reinterpret_cast<char*>(owner.object) + offset;
            Any  result;
            result.Assign(ptr, type, owner.ctx);
            return result;
        }

        // write (POD only)
        inline void MemberData::Set(Any owner, Any value) const
        {
            assert(owner.ctx && owner.ctx->types.count(type));
            assert(value.type == type);
            std::size_t sz = owner.ctx->types.at(type).size;
            std::memcpy(reinterpret_cast<char*>(owner.object) + offset, value.object, sz);
        }

        template<typename T>
        inline void MemberData::Set(const Any& owner, const T& v) const
        {
            static_assert(std::is_same_v<std::remove_const_t<T>, T>, "T must not be const-qualified");
            // ensure the TypeID matches
            assert(owner.ctx);
            assert(type == TypeIndex<T>::value);

            // compute the target address
            char* base = reinterpret_cast<char*>(owner.object);
            T* ptr = reinterpret_cast<T*>(base + offset);
            *ptr = v;  // direct assignment
        }

        // Resolve a type into its TypeData handle
        inline TypeData& Resolve(Context& ctx, TypeID id)
        {
            return ctx.types.at(id);
        }
        template<typename T>
        TypeData& Resolve(Context& ctx)
        {
            return Resolve(ctx, TypeIndex<T>::value);
        }

        template<typename T>
        struct Register
        {
            Context& ctx;
            TypeData& td;

            Register(Context& c)
                : ctx(c), td(ctx.types[TypeIndex<T>::value] = TypeData{TypeIndex<T>::value, typeid(T).name(), sizeof(T), alignof(T), {} }) {}

            // Register a data‐member pointer + name
            template<auto MemberPtr>
            Register& Data(const char* name)
            {
                using MemberT = std::remove_reference_t<decltype(std::declval<T>().*MemberPtr)>;

                MemberData md; 
                md.name = name;
                md.offset = reinterpret_cast<std::size_t>(&(reinterpret_cast<T*>(0)->*MemberPtr));
                md.type = TypeIndex<MemberT>::value;
                td.members.push_back(md);
                return *this;
            }
        };

        template<typename T>
        inline Any ForwardAsMeta(Context& ctx, T& obj)
        {
            Any a;
            a.Assign(&obj, TypeIndex<T>::value, &ctx);
            return a;
        }

        template<typename T>
        inline Any ForwardAsMeta(Context& ctx, const T& obj)
        {
            Any a;
            // We tag it with the const-qualified TypeID
            // so Meta::Any::Is<const T>() returns true, not Is<T>()
            a.Assign(const_cast<T*>(std::addressof(obj)), TypeIndex<const T>::value, &ctx);
            return a;
        }

        void Reset(Context& ctx)
        {
            ctx.types.clear();
        }
    }
}