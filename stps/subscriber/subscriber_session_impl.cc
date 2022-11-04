#include <stps/subscriber/subscriber_session_impl.h>

#include <stps/protocol_handshake_message.h>

#include "endian.h"
#include <iostream>
namespace stps
{
SubscriberSessionImpl::SubscriberSessionImpl(const std::shared_ptr<asio::io_service>& io_service, 
        const std::string& address, uint16_t port, int max_reconnection_attempts,
        const std::function<std::shared_ptr<std::vector<char>>()>& get_buffer_handler,
        const std::function<void(const std::shared_ptr<SubscriberSessionImpl>&)>& session_closed_handler)
    : address_(address)
    , port_(port)
    , resolver_(*io_service)
    , max_reconnection_attempts_(max_reconnection_attempts)
    , retries_left_(max_reconnection_attempts)
    , retry_timer_(*io_service, std::chrono::seconds(1))
    , canceled_(false)
    , data_socket_(*io_service)
    , data_strand_(*io_service)
    , get_buffer_handler_(get_buffer_handler)
    , session_closed_handler_(session_closed_handler)
{

}

SubscriberSessionImpl::~SubscriberSessionImpl()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::string thread_id = ss.str();
    std::cout << "SubscriberSession " << endpointToString() << ": Deleting from thread" << thread_id + "...\n";
    cancel();
}

void SubscriberSessionImpl::start()
{
    if (canceled_) return;
    resolveEndpoint();
}

void SubscriberSessionImpl::resolveEndpoint()
{
    asio::ip::tcp::resolver::query query(address_, std::to_string(port_));

    if (canceled_)
    {
        connectionFailedHandler();
        return;
    }

    resolver_.async_resolve(query,
            [me = shared_from_this()](system::error_code ec, 
            const asio::ip::tcp::resolver::iterator& resolved_endpoints)
            {
                if (ec)
                {
                    std::cout << "SubscriberSession " << me->endpointToString() 
                        << ": Failed to resolve address: " << ec.message() << std::endl;
                    me->connectionFailedHandler();
                }
                else
                {
                    me->connectToEndpoint(resolved_endpoints);
                }
            });
}

void SubscriberSessionImpl::connectToEndpoint(const asio::ip::tcp::resolver::iterator& resolved_endpoints)
{
    if (canceled_)
    {
        connectionFailedHandler();
        return;
    }

    auto endpoint_to_connect_to = resolved_endpoints->endpoint();
    for (auto it = resolved_endpoints; it != asio::ip::tcp::resolver::iterator(); it++)
    {
        if (it->endpoint().address().is_loopback())
        {
            endpoint_to_connect_to = it->endpoint();
            break;
        }
    }

    endpoint_ = endpoint_to_connect_to;
    data_socket_.async_connect(endpoint_to_connect_to, 
            [me = shared_from_this()](system::error_code ec)
            {
                if (ec)
                {
                    std::cout << "SubscriberSession " << me->endpointToString() 
                    << " Failed connecting to publisher: " + ec.message() << std::endl;
                    me->connectionFailedHandler();
                    return;
                    }
                else
                {
                    std::cout << "SubscriberSession " << me->endpointToString()
                    << ": Successfully connected to publisher " << me->endpointToString() << std::endl;
                    {
                        system::error_code nodelay_ec;
                        me->data_socket_.set_option(asio::ip::tcp::no_delay(true), nodelay_ec);
                        if (nodelay_ec)
                        {
                            std::cout << "SubscriberSession " << me->endpointToString() 
                            << ": Failed setting tcp::no_delay option. The performance may suffer\n";
                        }
                        me->sendProtokolHandshakeRequest();
                    }
                }
            });
}

void SubscriberSessionImpl::sendProtokolHandshakeRequest()
{
    if (canceled_)
    {
        connectionFailedHandler();
        return;
    }

    std::cout << "SubscriberSession " << endpointToString() 
        << ": Sending ProtocolHandshakeRequest.\n";

    std::shared_ptr<std::vector<char>> buffer = std::make_shared<std::vector<char>>();
    buffer->resize(sizeof(TCPHeader) + sizeof(ProtocolHandshakeMessage));

    TCPHeader* header = reinterpret_cast<TCPHeader*>(buffer->data());
    header->header_size = htole16(sizeof(TCPHeader));
    header->type = MessageContentType::ProtocolHandshake;
    header->reserved = 0;
    header->data_size = htole64(sizeof(ProtocolHandshakeMessage));

    ProtocolHandshakeMessage* handshake_message = 
        reinterpret_cast<ProtocolHandshakeMessage*>(&(buffer->operator[](sizeof(TCPHeader))));
    handshake_message->protocol_version = 0;

    asio::async_write(data_socket_, asio::buffer(*buffer), data_strand_.wrap(
                [me = shared_from_this(), buffer](system::error_code ec, std::size_t)
                {
                    if (ec)
                    {
                        std::cout << "SubscriberSession " << me->endpointToString() 
                        << "Failed sending ProtocolHandshakeRequest: " << ec.message();
                        me->connectionFailedHandler();
                        return;
                    }
                    me->readHeaderLength();
                }));
}

void SubscriberSessionImpl::connectionFailedHandler()
{
    {
        system::error_code ec;
        data_socket_.close(ec);
    }

    if (!canceled_ && (retries_left_ < 0 || retries_left_ > 0))
    {
        if (retries_left_ > 0)
        {
            retries_left_--;
        }

        retry_timer_.async_wait([me = shared_from_this()](system::error_code ec)
                {
                    if (ec)
                    {
                        std::cout << "SubscriberSession " << me->endpointToString() 
                        << ": Waiting to reconnect failed: " << ec.message() << std::endl;
                        return;
                    }
                    me->resolveEndpoint();
                });
    }
    else
    {
        session_closed_handler_(shared_from_this());
    }
}

void SubscriberSessionImpl::readHeaderLength()
{
    if (canceled_)
    {
        connectionFailedHandler();
        return;
    }
    
    std::shared_ptr<TCPHeader> header = std::make_shared<TCPHeader>();

    asio::async_read(data_socket_,
            asio::buffer(&(header->header_size), sizeof(header->header_size)),
            asio::transfer_at_least(sizeof(header->header_size)),
            data_strand_.wrap([me = shared_from_this(), header](system::error_code ec, std::size_t)
                {
                    if (ec)
                    {
                        std::cout << "SubscriberSession " << me->endpointToString() 
                        << ": Error reading header length: " << ec.message() << std::endl;
                        me->connectionFailedHandler();
                        return;
                    }
                    me->readHeaderContent(header);
                }));
}

void SubscriberSessionImpl::readHeaderContent(const std::shared_ptr<TCPHeader>& header)
{
    if (canceled_)
    {
        connectionFailedHandler();
        return;
    }

    if (header->header_size < sizeof(header->header_size))
    {
        std::cout << "SubscriberSession " << endpointToString() 
            << ": Received header length of " << std::to_string(header->header_size) 
            << ", which is less than the minimal header size.\n";
        connectionFailedHandler();
        return;
    }

    const uint16_t remote_header_size = le16toh(header->header_size);
    const uint16_t my_header_size = sizeof(*header);
    const uint16_t bytes_to_read_from_socket = 
        std::min(remote_header_size, my_header_size) - sizeof(header->header_size);
    const uint16_t bytes_to_discard_from_socket = 
        (remote_header_size < my_header_size ? (remote_header_size - my_header_size) : 0);

    asio::async_read(data_socket_, 
            asio::buffer(&reinterpret_cast<char*>(header.get())[sizeof(header->header_size)], bytes_to_read_from_socket),
            asio::transfer_at_least(bytes_to_read_from_socket),
            data_strand_.wrap([me = shared_from_this(), header, bytes_to_discard_from_socket](system::error_code ec, std::size_t)
                    {
                        if (ec)
                        {
                            std::cout << "SubscriberSession " << me->endpointToString()
                            << ": Error reading header content: " << ec.message();
                            me->connectionFailedHandler();
                            return;
                        }
                        
                        std::cout << "SubscriberSession " << me->endpointToString() 
                        << ": Received header content: " << "data_size: " 
                        << std::to_string(le64toh(header->data_size));

                        if (bytes_to_discard_from_socket > 0)
                        {
                            me->discardDataBetweenHeaderAndPayload(header, 
                                    bytes_to_discard_from_socket);
                        }
                        else
                        {
                            me->readPayload(header);
                        }
                    }));
}

void SubscriberSessionImpl::discardDataBetweenHeaderAndPayload(const std::shared_ptr<TCPHeader>& header, 
        uint16_t bytes_to_discard)
{
    if (canceled_)
    {
        connectionFailedHandler();
        return;
    }
    
    std::vector<char> data_to_discard;
    data_to_discard.resize(bytes_to_discard);

    std::cout << "SubscriberSession " << endpointToString() << ": Discarding " 
        << std::to_string(bytes_to_discard) << " bytes after the header.\n";
    
    asio::async_read(data_socket_,
            asio::buffer(data_to_discard.data(), bytes_to_discard),
            asio::transfer_at_least(bytes_to_discard),
            data_strand_.wrap([me = shared_from_this(), header](system::error_code ec, std::size_t)
                {
                    if (ec)
                    {
                        std::cout << "SubscriberSession " << me->endpointToString()
                        << ": Error discarding bytes after header: "
                        << ec.message();

                        me->connectionFailedHandler();
                        return;
                    }
                    me->readPayload(header);
                }));
}

void SubscriberSessionImpl::readPayload(const std::shared_ptr<TCPHeader>& header)
{
    if (canceled_)
    {
        connectionFailedHandler();
        return;
    }

    if (header->data_size == 0)
    {
        std::cout << "SubscriberSession " << endpointToString() 
            << ": Received data size of 0.\n";
        readHeaderLength();
        return;
    }

    std::shared_ptr<std::vector<char>> data_buffer = get_buffer_handler_();

    if (data_buffer->capacity() < le64toh(header->data_size))
    {
        data_buffer->reserve(static_cast<size_t>(le64toh(header->data_size) * 1.1));
    }

    data_buffer->resize(le64toh(header->data_size));

    asio::async_read(data_socket_,
            asio::buffer(data_buffer->data(), le64toh(header->data_size)),
            asio::transfer_at_least(le64toh(header->data_size)),
            data_strand_.wrap([me = shared_from_this(), header, data_buffer](system::error_code ec, std::size_t)
                {
                    if (ec)
                    {
                        std::cout << "SubscriberSession " << me->endpointToString() 
                        << ": Error reading payload: " << ec.message() << std::endl;
                        me->connectionFailedHandler();
                        return;
                    }

                    me->retries_left_ = me->max_reconnection_attempts_;

                    if (header->type == MessageContentType::ProtocolHandshake)
                    {
                        ProtocolHandshakeMessage handshake_message;
                        size_t bytes_to_copy = std::min(data_buffer->size(), sizeof(ProtocolHandshakeMessage));
                        std::memcpy(&handshake_message, data_buffer->data(), bytes_to_copy);
                        std::cout << "SubscriberSession " << me->endpointToString() << 
                        ": Received Handshake message. Using Protocol Version v" 
                        << std::to_string(handshake_message.protocol_version) << std::endl;
                    }
                    else if (header->type == MessageContentType::RegularPayload)
                    {
                        me->data_strand_.post([me, data_buffer, header]()
                                {
                                    if (me->canceled_)
                                    {
                                        me->connectionFailedHandler();
                                        return;
                                    }
                                    me->synchronous_callback_(data_buffer, header);
                                });
                    }
                    else
                    {
                        std::cout << "SubscriberSession " << me->endpointToString() 
                            << ": Received message has unknown type: " 
                            << std::to_string(static_cast<int>(header->type)) << std::endl;
                    }

                    me->data_strand_.post([me]()
                            {
                                me->readHeaderLength();
                            });
                }));
}

void SubscriberSessionImpl::setSynchronousCallback(const std::function<void(const std::shared_ptr<std::vector<char>>&, 
            const std::shared_ptr<TCPHeader>&)>& callback)
{
    if (canceled_) return;
    data_strand_.post([me = shared_from_this(), callback]()
            {
                me->synchronous_callback_ = callback;
            });
}

std::string SubscriberSessionImpl::getAddress() const
{
    return address_;
}

uint16_t SubscriberSessionImpl::getPort() const
{
    return port_;
}

void SubscriberSessionImpl::cancel()
{
    bool already_canceled = canceled_.exchange(true);
    if (already_canceled) return;
    std::cout << "SubscriberSession " << endpointToString() << ": Cancelling..." << std::endl;
    
    {
        system::error_code ec;
        data_socket_.close(ec);
        if (ec)
            std::cout << "SubscriberSession " << endpointToString() + ": Failed closing socket: " << ec.message() << std::endl;
        else
            std::cout << "SubscriberSession " << endpointToString() + ": Susscessfully closed socket." << std::endl;
    }

    {
        system::error_code ec;
        data_socket_.cancel(ec);
        if (ec)
            std::cout << "SubscriberSession " << endpointToString()
                << ": Failed cancelling socket: " + ec.message() << std::endl;
        else
            std::cout << "SubscriberSession " << endpointToString() + ": Sus    scessfully canceled socket." << std::endl;
    }

    {
        system::error_code ec;
        retry_timer_.cancel(ec);
    }

    resolver_.cancel();
}

bool SubscriberSessionImpl::isConnected() const
{
    system::error_code ec;
    data_socket_.remote_endpoint(ec);

    if (ec)
        return false;
    else
        return true;
}

std::string SubscriberSessionImpl::remoteEndpointToString() const
{
    return address_ + ":" + std::to_string(port_);
}

std::string SubscriberSessionImpl::localEndpointToString() const
{
    system::error_code ec;
    auto local_endpoint = data_socket_.local_endpoint(ec);
    if (ec)
        return "?";
    else
        return local_endpoint.address().to_string() + ":" + std::to_string(local_endpoint.port());
}

std::string SubscriberSessionImpl::endpointToString() const
{
    return localEndpointToString() + "->" + remoteEndpointToString();
}

} // namespace stps
