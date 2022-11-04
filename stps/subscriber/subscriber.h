#pragma once

#include <stps/executor/executor.h>
#include <stps/subscriber/subscriber_session.h>
#include <stps/callback_data.h>

#include <functional>
#include <stdint.h>
#include <memory>
#include <chrono>
#include <string>

namespace stps
{
class SubscriberImpl;

class Subscriber
{
    public:
        Subscriber(const std::shared_ptr<Executor>& executor);
        Subscriber(const Subscriber&) = default;
        Subscriber& operator=(const Subscriber&) = default;
        Subscriber& operator=(Subscriber&&) = default;
        Subscriber(Subscriber&&) = default;

       ~Subscriber();

       std::shared_ptr<SubscriberSession> addSession(const std::string& address, uint16_t port, 
               int max_reconnection_attemps = -1);
       std::vector<std::shared_ptr<SubscriberSession>> getSessions() const;
       void setCallback(const std::function<void(const CallbackData& callback_data)>& callback_function, 
               bool synchronous_execution = false);
       void clearCallback();
       void cancel();
    private:
       std::shared_ptr<SubscriberImpl> subscriber_impl_;
};
} // namespace stps
