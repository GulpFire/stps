#pragma once

#include <stdint.h>
#include <memory>
#include <string>

namespace stps
{
class SubscriberSessionImpl;
class SubscriberImpl;

class SubscriberSession
{
    friend SubscriberImpl;

    public:
    SubscriberSession(const SubscriberSession&) = default;
    SubscriberSession& operator=(const SubscriberSession&) = default;
    SubscriberSession& operator=(SubscriberSession&) = default;
    SubscriberSession(SubscriberSession&&) = default;

    std::string getAddress() const;
    uint16_t getPort() const;
    void cancel();
    bool isConnected() const;

    private:
    std::shared_ptr<SubscriberSessionImpl> subscriber_session_impl_;
    SubscriberSession(const std::shared_ptr<SubscriberSessionImpl>& impl);
};
} // namespace stps
