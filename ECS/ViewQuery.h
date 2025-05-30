#pragma once

#include "IComponentStorage.h"

namespace HBL2
{
    template<typename Component>
    class ViewQuery
    {
    public:
        ViewQuery(IComponentStorage* storage)
            : m_Storage(storage)
        {

        }

        ViewQuery& ForEach(std::function<void(Component&)>&& func)
        {
            m_Function = MakeRawCallback<Component>(&func);
            return *this;
        }

        void Run()
        {
            m_Storage->IterateRaw(m_Function);
        }

        void Schedule()
        {

        }

        void Dispatch()
        {

        }

    private:
        IComponentStorage* m_Storage = nullptr;
        TrampolineFunction<void, void*> m_Function;
    };
}