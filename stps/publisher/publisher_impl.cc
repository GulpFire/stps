#include <stps/publisher/publisher_impl.h>
#include <stps/tcp_header.h>
#include <stps/executor/executor_impl.h>
#include <iostream>
#include "endian.h"

#include <asio/socket_base.hpp>

namespace stps
{
PublisherImpl::PublisherImpl(const std::shared_ptr<Executor>& executor)
    : is_running_(false)
    , executor_(executor)
    , acceptor_(*executor_->executor_impl_->ioService())
{

}

PublisherImpl::~PublisherImpl()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::string thread_id = ss.str();
    std::cout << "Publisher " << localEndpointToString() << ": Deleting from thread " << thread_id << std::endl;

    if (is_running_)
    {
        cancel();
    }
    std::cout << "Publisher " << localEndpointToString() << ": Deleted." << std::endl;
}

bool PublisherImpl::start(const std::string& address, uint16_t port)
{
    asio::error_code make_address_ec;
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(address, 
                make_address_ec), port);
    if (make_address_ec)
    {
        std::cout << "Publisher: Error parsing address \"" << address 
            << ":" << std::to_string(port) << "\": " << make_address_ec.message()
            << std::endl;
        return false;
    }

    {
        asio::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            std::cout << "Publisher " << toString(endpoint) 
                << ": Error opening acceptor: " << ec.message() << std::endl;
            return false;
        }
    }

    {
        asio::error_code ec;
        acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
        if (ec)
        {
            std::cout << "Publisher " << toString(endpoint) 
                << ": Error setting reuse_address option: " << ec.message() << std::endl;
            return false;
        }
    }

    {
        asio::error_code ec;
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            std::cout << "Publisher " << toString(endpoint) << ": Error binding acceptor: " 
                << ec.message();
            return false;
        }
    }

    {
        asio::error_code ec;
        acceptor_.listen(asio::socket_base::max_listen_connections, ec);
        if (ec)
        {
            std::cout << "Publisher " << toString(endpoint) 
                << ": Error listening on acceptor: " << ec.message() << std::endl;
            return false;
        }
    }

    is_running_ = true;
    acceptClient();
    
    return true;
}

void PublisherImpl::cancel()
{
    {
        asio::error_code ec;
        acceptor_.close(ec);
        acceptor_.cancel(ec);
    }

    is_running_ = false;

    std::vector<std::shared_ptr<PublisherSession>> publisher_sessions;
    {
        std::lock_guard<std::mutex> publisher_sessions_lock(publisher_sessions_mtx_);
        publisher_sessions = publisher_sessions_;
    }

    for (const auto& session : publisher_sessions)
    {
        session->cancel();
    }
}

void PublisherImpl::acceptClient()
{
    std::function<void(const std::shared_ptr<PublisherSession>&)> publisher_session_closed_handler =
        [me = shared_from_this()](const std::shared_ptr<PublisherSession>& session) -> void
        {
            std::lock_guard<std::mutex> publisher_sessions_lock(me->publisher_sessions_mtx_);
            auto session_it = std::find(me->publisher_sessions_.begin(), 
                    me->publisher_sessions_.end(), session);
            if (session_it != me->publisher_sessions_.end())
            {
                me->publisher_sessions_.erase(session_it);
                std::cout << "Publisher " << me->localEndpointToString()
                    << ": Successfully removed Session to subscriber "
                    << session->remoteEndpointToString() 
                    << ". Current subscriber count: " 
                    << std::to_string(me->publisher_sessions_.size()) << "."
                    << std::endl;
            }
            else
            {
                std::cout << "Publisher " << me->localEndpointToString()
                    << ": Tring to delete a non-exsiting publisher session"
                    << std::endl;
            }
        };

    auto session = std::make_shared<PublisherSession>(executor_->executor_impl_->ioService(), 
            publisher_session_closed_handler);
    acceptor_.async_accept(session->getSocket(), 
            [session, me = shared_from_this()](asio::error_code ec)
            {
                if (ec)
                {
                    std::cout << "Publisher " << me->localEndpointToString()
                    << ": Error while waiting for subscriber: " << ec.message()
                    << std::endl;
                    return;
                }
                else
                {
                    std::cout << "Publisher " << me->localEndpointToString()
                    << ": Subscriber " << session->remoteEndpointToString()
                    << " has connected." << std::endl;
                }

                session->start();

                {
                    std::lock_guard<std::mutex> publisher_sessions_lock_(me->publisher_sessions_mtx_);
                    me->publisher_sessions_.push_back(session);
                }

                me->acceptClient();
            });
}

bool PublisherImpl::send(const std::vector<std::pair<const char* const, const size_t>>& payloads)
{
    if (!is_running_)
    {
        std::cout << "Publisher::send " << localEndpointToString() 
            << ": Tried to send data to a non-running Publisher" << std::endl;
        return false;
    }

    {
        std::lock_guard<std::mutex> publisher_sessions_lock(publisher_sessions_mtx_);
        if (publisher_sessions_.empty())
        {
            std::cout << "Publisher::send " << localEndpointToString()
                << ": No connection to any subscriber. Skip sending data."
                << std::endl;
            return true;
        }
    }

    std::shared_ptr<std::vector<char>> buffer = buffer_pool_.allocate();
    
    std::stringstream buffer_pointer_ss;
    buffer_pointer_ss << "0x" << std::hex << buffer.get();
    const std::string buffer_pointer_string = buffer_pointer_ss.str();

    {
        size_t header_size = sizeof(TCPHeader);
        size_t entire_payload_size = 0;
        for (const auto& payload : payloads)
        {
            entire_payload_size += payload.second;
        }

        const size_t compelete_size = header_size + entire_payload_size;

        if (buffer->capacity() < compelete_size)
        {
            buffer->reserve(static_cast<size_t>(compelete_size * 1.1));
        }

        buffer->resize(compelete_size);

        auto header = reinterpret_cast<stps::TCPHeader*>(&(*buffer)[0]);
        header->header_size = htole16(sizeof(TCPHeader));
        header->type = MessageContentType::RegularPayload;
        header->reserved = 0;
        header->data_size = htole64(entire_payload_size);

        size_t current_position = header_size;
        for (const auto& payload : payloads)
        {
            if (payload.first && (payload.second > 0))
            {
                memcpy(&((*buffer)[current_position]), payload.first, payload.second);
                current_position += payload.second;
            }
        }
    }

    {
        std::lock_guard<std::mutex> publisher_sessions_lock(publisher_sessions_mtx_);
        for (const auto& publisher_session : publisher_sessions_)
        {
            publisher_session->sendDataBuffer(buffer);
        }
    }

    return true;
}

uint16_t PublisherImpl::getPort() const
{
    if (is_running_)
    {
        asio::error_code ec;
        auto local_endpoint = acceptor_.local_endpoint();
        if (!ec)
            return local_endpoint.port();
        else
            return 0;
    }
    else
    {
        return 0;
    }
}

size_t PublisherImpl::getSubscriberCount() const
{
    std::lock_guard<std::mutex> publisher_sessions_lock(publisher_sessions_mtx_);
    return publisher_sessions_.size();
}

bool PublisherImpl::isRunning() const
{
    return is_running_;
}

std::string PublisherImpl::toString(const asio::ip::tcp::endpoint& endpoint) const
{
    return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
}

std::string PublisherImpl::localEndpointToString() const
{
    asio::error_code ec;
    auto local_endpoint = acceptor_.local_endpoint(ec);
    if (!ec)
        return toString(local_endpoint);
    else
        return "?";
}

} // namespace stps
