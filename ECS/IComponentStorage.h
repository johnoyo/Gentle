#pragma once

#include "ComponentMask.h"
#include "Utilities/Collections/Span.h"
#include "Utilities/Collections/TrampolineFunction.h"

namespace HBL2
{
    class IComponentStorage
    {
    public:
        virtual ~IComponentStorage() = default;

        virtual void* Add(Entity e) = 0;
        virtual void Remove(Entity e) = 0;
        virtual void* Get(Entity e) = 0;
        virtual bool Has(Entity e) = 0;

        virtual ComponentMaskAVX& Mask() const = 0;
        virtual const Span<const Entity> Indices() const = 0;

        virtual void Clear() = 0;

        virtual void IterateRaw(TrampolineFunction<void, void*>& callback) const = 0;
    };
}