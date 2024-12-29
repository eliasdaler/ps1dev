#pragma once

#include <EASTL/map.h>
#include <EASTL/string_view.h>

#include <Core/StringHash.h>

#include <Graphics/Model.h>
#include <Graphics/TextureInfo.h>

#include <common/syscalls/syscalls.h>

#define DEBUG_RESOURCE_LOAD

struct ResourceCache {
    template<typename T>
    struct Resource {
        T value;
        int refCount{0};
        bool persistent{false};
    };

    template<typename T>
    using ResourceContainer = eastl::map<StringHash, Resource<T>>;

    template<typename T>
    ResourceContainer<T>& getResourceContainter()
    {
        if constexpr (eastl::is_same<T, TextureInfo>()) {
            return textures;
        } else if constexpr (eastl::is_same<T, Model>()) {
            return models;
        } else {
            static_assert(false, "No resource container for the type");
        }
    }

    template<typename T>
    const ResourceContainer<T>& getResourceContainter() const
    {
        if constexpr (eastl::is_same<T, TextureInfo>()) {
            return textures;
        } else if constexpr (eastl::is_same<T, Model>()) {
            return models;
        } else {
            static_assert(false, "No resource container for the type");
        }
    }

    template<typename T>
    void refResource(StringHash hash)
    {
        auto& container = getResourceContainter<T>();
        const auto it = container.find(hash);
        if (it != container.end()) {
            ++it->second.refCount;
#ifdef DEBUG_RESOURCE_LOAD
            ramsyscall_printf("ref '%s', ref count= %d\n", hash.getStr(), it->second.refCount);
#endif
        }
    }

    template<typename T>
    void derefResource(StringHash hash)
    {
        auto& container = getResourceContainter<T>();
        const auto it = container.find(hash);
        if (it != container.end()) {
            --it->second.refCount;
#ifdef DEBUG_RESOURCE_LOAD
            ramsyscall_printf("deref '%s', ref count= %d\n", hash.getStr(), it->second.refCount);
#endif
        }
    }

    template<typename T>
    void removeUnusedResources()
    {
        auto& container = getResourceContainter<T>();
        eastl::erase_if(container, [](auto& v) {
#ifdef DEBUG_RESOURCE_LOAD
            if (!v.second.persistent && v.second.refCount <= 0) {
                ramsyscall_printf("[!] Removing '%s': (ref == 0)\n", v.first.getStr());
            }
#endif
            return v.second.refCount <= 0;
        });
    }

    template<typename T>
    bool resourceLoaded(StringHash hash) const
    {
        const auto& container = getResourceContainter<T>();
        return container.find(hash) != container.end();
    }

    template<typename T>
    void putResource(StringHash hash, T&& value)
    {
#ifdef DEBUG_RESOURCE_LOAD
        if (resourceLoaded<T>(hash)) {
            ramsyscall_printf("[!!!!] Error '%s' was already loaded\n");
            return;
        }
#endif
        auto& container = getResourceContainter<T>();
        container.emplace(hash, Resource<T>{.value = eastl::move(value), .refCount = 1});
    }

    template<typename T>
    void setPersistent(StringHash hash, bool b)
    {
        auto& container = getResourceContainter<T>();
        const auto it = container.find(hash);
        if (it != container.end()) {
            it->second.persistent = b;
#ifdef DEBUG_RESOURCE_LOAD
            ramsyscall_printf(
                "Resource '%s' persistent: %d\n", hash.getStr(), (int)it->second.persistent);
#endif
        }
    }

    template<typename T>
    const T& getResource(StringHash hash) const
    {
        auto& container = getResourceContainter<T>();
        const auto it = container.find(hash);

#ifdef DEBUG_RESOURCE_LOAD
        if (it == container.end()) {
            ramsyscall_printf("Resource '%s' was not loaded", hash.getStr());
        }
#endif

        return it->second.value;
    }

    ResourceContainer<TextureInfo> textures;
    ResourceContainer<Model> models;
};
