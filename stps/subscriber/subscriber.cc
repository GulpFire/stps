#include <stps/subscriber/subscriber.h>
#include <stps/subscriber/subscriber_impl.h>
#include <stps/executor/executor.h>
#include <stps/executor/executor_impl.h>

namespace stps
{
Subscriber::Subscriber(const std::shared_ptr<Executor>& executor)
    : subscriber_impl_(std::make_shared<SubscriberImpl>(executor))
{

}

Subscriber::~Subscriber()
{
    subscriber_impl_->cancel();
}

std::shared_ptr<SubscriberSession> Subscriber::addSession(const std::string& address, uint16_t port,
        int max_reconnection_attempts)
{
    return subscriber_impl_->addSession(address, port, max_reconnection_attempts);
}

std::vector<std::shared_ptr<SubscriberSession>> Subscriber::getSessions() const
{
    return subscriber_impl_->getSessions();
}

void Subscriber::setCallback(const std::function<void(const CallbackData& callback_data)>& callback_function, bool synchronous_execution)
{
    subscriber_impl_->setCallback(callback_function, synchronous_execution);
}

void Subscriber::clearCallback()
{
    subscriber_impl_->setCallback([](const auto&){}, true);
}

void Subscriber::cancel()
{
    subscriber_impl_->cancel();
}
} // namespace stps

