#ifndef SERVICES_NAME_LOOKUP_HPP
#define SERVICES_NAME_LOOKUP_HPP

#include "services/network/NameResolver.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace services
{
    class NameLookup
        : public NameResolver
    {
    public:
        NameLookup();
        ~NameLookup();

        virtual void Lookup(NameResolverResult& result) override;
        virtual void CancelLookup(NameResolverResult& result) override;

    private:
        void Run();
        void LookupIPv4();

    private:
        std::mutex mutex;
        std::condition_variable condition;
        infra::IntrusiveList<NameResolverResult> nameLookup;
        bool run = true;
        std::thread lookupThread;
        NameResolverResult* currentLookup = nullptr;
    };
}

#endif
