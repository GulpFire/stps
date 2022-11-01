#include <stps/publisher/publisher.h>
#include <stps/publisher/publisher_impl.h>
#include <stps/executor/executor_impl.h>

namespace stps
{
Publisher::Publisher(const std::shared_ptr<Executor>& executor, const std::string& address, uint16_t port)
{
    publisher_impl_->start(address, port);        
}

Publisher::Publisher(const std::shared_ptr<Executor>& executor, uint16_t port)
    : Publisher(executor, "0.0.0.0", port)
{

}

Publisher::~Publisher()
{
    publisher_impl_->cancel();
}

uint16_t Publisher::getPort() const
{
    return publisher_impl_->getPort();
}

size_t Publisher::getSubscriberCount() const
{
    return publisher_impl_->getSubscriberCount();
}

bool Publisher::isRunning() const
{
    return publisher_impl_->isRunning();
}

bool Publisher::send(const char* const data, size_t size) const
{
    return this->send({{data, size}});
}

void Publisher::send(const std::vector<std::pair<const char* const, const size_t>>& payloads) const
{
    return publisher_impl_->send(payloads);
}

void Publisher::cancel()
{
    publisher_impl_->cancel();
}

} // namespace stps
