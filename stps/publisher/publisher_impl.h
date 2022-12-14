#pragma once

#include <stps/executor/executor.h>
#include <stps/publisher/publisher_session.h>
#include <boost/asio.hpp>
#include <recycle/shared_pool.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

using namespace boost;

namespace stps
{

class PublisherImpl : public std::enable_shared_from_this<PublisherImpl>
{
    public:
        PublisherImpl(const std::shared_ptr<Executor>& executor);

        PublisherImpl(const PublisherImpl&) = delete;
        
        PublisherImpl& operator=(const PublisherImpl&) = delete;
        
        PublisherImpl& operator=(PublisherImpl&&) = delete;
        
        PublisherImpl(PublisherImpl&&) = delete;

        ~PublisherImpl();

        bool start(const std::string& address, uint16_t port);

        void cancel();

        bool send(const std::vector<std::pair<const char* const, const size_t>>& payloads);

        uint16_t getPort() const;
        
        size_t getSubscriberCount() const;
       
        bool isRunning() const;

    private:
        std::atomic<bool> is_running_;
        const std::shared_ptr<Executor> executor_;
        asio::ip::tcp::acceptor acceptor_;
        mutable std::mutex publisher_sessions_mtx_;
        std::vector<std::shared_ptr<PublisherSession>> publisher_sessions_;

        struct BufferPoolLockPolicy
        {
            using mutex_type = std::mutex;
            using lock_type = std::lock_guard<mutex_type>;
        };

        recycle::shared_pool<std::vector<char>, BufferPoolLockPolicy> buffer_pool_;

        void acceptClient();

        std::string toString(const asio::ip::tcp::endpoint& endpoint) const;

        std::string localEndpointToString() const;

};

} // namespace stps
