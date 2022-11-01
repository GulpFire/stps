#pragma once

#include <stps/executor/executor.h>

#include <stdint.h>

#include <memory>
#include <string>
#include <chrono>
#include <vector>

namespace stps
{
class PublisherImpl;

class  Publisher
{
    public:
        Publisher(const std::shared_ptr<Executor>& executor, const std::string& address, uint16_t port);
        Publisher(const std::shared_ptr<Executor>& excutor, uint16_t port = 0);
        Publisher(const Publisher&) = default;
        Publisher& operator=(Const Publisher&) = default;
        
        Publisher& operator=(Publisher&&) = default;
        Publisher(Publisher&&) = default;
        
        ~Publisher();

        uint16_t getPort() const;
        size_t getSubscriberCount() const;
        bool isRunning() const;
        bool send(const char* const data, size_t size) const;
        bool send(const std::vector<std::pair<const char* const, const size_t>>& buffers) const;

        void cancel();

    private:
        std::shared_ptr<PublisherImpl> publisher_impl_;

};
} // namespace stps
