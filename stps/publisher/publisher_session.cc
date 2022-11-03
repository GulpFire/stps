#include <stps/publisher/publisher_session.h>
#include <stps/tcp_header.h>
#include <stps/protocol_handshake_message.h>
#include <thread>
#include <endian.h>

#include <iostream>

namespace stps
{
PublisherSession::PublisherSession(const std::shared_ptr<asio::io_service>& io_service,
        const std::function<void(const std::shared_ptr<PublisherSession>&)>& session_closed_handler)
    : io_service_(io_service)
    , state_(State::NotStarted)
    , session_closed_handler_(session_closed_handler)
    , data_socket_(*io_service_)
    , data_strand_(*io_service_)
    , sending_in_progress_(false)
{

}

PublisherSession::~PublisherSession()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::string thread_id = ss.str();
    std::cout << "PublisherSession " << endpointToString() << ": Deleting from thread "
        << thread_id << "...\n";
}

void PublisherSession::start()
{
    {
        asio::error_code ec;
        data_socket_.set_option(asio::ip::tcp::no_delay(true), ec);
        if (ec)
            std::cout << "PublisherSession " << endpointToString() 
                << ": Failed setting tcp::no_delay." << std::endl;
    }

    state_ = State::Handshaking;

    receiveTcpPacket();
}

void PublisherSession::cancel()
{
    sessionClosedHandler();
}

void PublisherSession::sessionClosedHandler()
{
    State previous_state = (state_.exchange(State::Canceled));
    if (previous_state == State::Canceled)
        return;

    {
        asio::error_code ec;
        data_socket_.close(ec);
    }

    session_closed_handler_(shared_from_this());
}

void PublisherSession::readHeaderLength()
{
    if (state_ == State::Canceled) return;

    std::shared_ptr<TCPHeader> header = std::make_shared<TCPHeader>();

    asio::async_read(data_socket_, 
            asio::buffer(&(header->header_size), sizeof(header->header_size)),
            asio::transfer_at_least(sizeof(header->header_size)),
            data_strand_.wrap([me = shared_from_this(), header](asio::error_code ec, std::size_t)
                {
                    if (ec)
                    {
                        me->sessionClosedHandler();
                        return;
                    }
                    me->readHeaderContent(header);
                }));
}

void PublisherSession::readHeaderContent(const std::shared_ptr<TCPHeader>& header)
{
    if (state_ == State::Canceled)
        return;

    if (header->header_size < sizeof(header->header_size))
    {
        sessionClosedHandler();
        return;
    }

    const uint16_t remote_header_size = le16toh(header->header_size);
    const uint16_t my_header_size = sizeof(*header);

    const uint16_t bytes_to_read_from_socket = 
        std::min(remote_header_size, my_header_size) - sizeof(header->header_size);
    const uint16_t bytes_to_discard_from_socket = 
        (remote_header_size > my_header_size ? (remote_header_size - my_header_size) : 0);

    asio::async_read(data_socket_,
            asio::buffer(&reinterpret_cast<char*>(header.get())[sizeof(header->header_size)], 
                bytes_to_read_from_socket),
            asio::transfer_at_least(bytes_to_read_from_socket),
            data_strand_.wrap([me = shared_from_this(), header,
            bytes_to_discard_from_socket](asio::error_code ec, std::size_t)
            {
                if (ec)
                {
                    me->sessionClosedHandler();
                    return;
                }
                if (bytes_to_discard_from_socket > 0)
                {
                    me->discardDataBetweenHeaderAndPayload(header, bytes_to_discard_from_socket);
                }
                else
                {
                    me->readPayload(header);
                }
            }));
}

void PublisherSession::discardDataBetweenHeaderAndPayload(const std::shared_ptr<TCPHeader>& header, 
        uint16_t bytes_to_discard)
{
    if (state_ == State::Canceled)
        return;

    std::vector<char> data_to_discard;
    data_to_discard.resize(bytes_to_discard);

    asio::async_read(data_socket_,
            asio::buffer(data_to_discard.data(), bytes_to_discard),
            asio::transfer_at_least(bytes_to_discard),
            data_strand_.wrap([me = shared_from_this(), header](asio::error_code ec, std::size_t)
                {
                    if (ec)
                    {
                        me->sessionClosedHandler();
                        return;
                    }
                    me->readPayload(header);
                }));
}

void PublisherSession::readPayload(const std::shared_ptr<TCPHeader>& header)
{
    if (state_ == State::Canceled)
        return;

    if (header->data_size == 0)
    {
        sessionClosedHandler();
        return;
    }

    std::shared_ptr<std::vector<char>> data_buffer = 
        std::make_shared<std::vector<char>>();
    data_buffer->resize(le64toh(header->data_size));

    asio::async_read(data_socket_,
            asio::buffer(data_buffer->data(), le64toh(header->data_size)),
            asio::transfer_at_least(le64toh(header->data_size)),
            data_strand_.wrap([me = shared_from_this(), header,
                data_buffer](asio::error_code ec, std::size_t)
                {
                    if (ec)
                    {
                        me->sessionClosedHandler();
                        return;
                    }

                    if (header->type == MessageContentType::ProtocolHandshake)
                    {
                        ProtocolHandshakeMessage handshake_message;
                        size_t bytes_to_copy = std::min(data_buffer->size(),
                                sizeof(ProtocolHandshakeMessage));
                        std::memcpy(&handshake_message, data_buffer->data(),
                                bytes_to_copy);
                        me->sendProtocolHandshakeResponse();
                    }
                    else
                    {
                        me->sessionClosedHandler();
                    }
                }));
}

void PublisherSession::sendProtocolHandshakeResponse()
{
    if (state_ == State::Canceled) return;

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

    sendBufferToClient(buffer);
    State old_state = state_.exchange(State::Running);
    if (old_state != State::Handshaking) state_ = old_state;
}

void PublisherSession::sendDataBuffer(const std::shared_ptr<std::vector<char>>& buffer)
{
    if (state_ == State::Canceled) return;

    {
        std::lock_guard<std::mutex> next_buffer_lock(next_buffer_mutex_);

        if ((state_ == State::Running) && !sending_in_progress_)
        {
            sending_in_progress_ = true;
            sendBufferToClient(buffer);
        }
        else
        {
            next_buffer_to_send_ = buffer;
        }
    }
}

void PublisherSession::sendBufferToClient(const std::shared_ptr<std::vector<char>>& buffer)
{
    if (state_ == State::Canceled) return;

    asio::async_write(data_socket_,
            asio::buffer(*buffer),
            data_strand_.wrap([me = shared_from_this(), buffer](asio::error_code ec, std::size_t)
                {
                    if (ec)
                    {
                        me->sessionClosedHandler();
                        return;
                    }

                    if (me->state_ == State::Canceled)
                    {
                        return;
                    }

                    {
                        std::lock_guard<std::mutex> next_buffer_lock(me->next_buffer_mutex_);
                        if (me->next_buffer_to_send_)
                        {
                            auto next_buffer_tmp = me->next_buffer_to_send_;
                            me->next_buffer_to_send_ = nullptr;
                            me->sendBufferToClient(next_buffer_tmp);
                        }
                        else
                        {
                            me->sending_in_progress_ = false;
                        }
                    }
                }
                
                ));
}

asio::ip::tcp::socket& PublisherSession::getSocket()
{
    return data_socket_;
}

std::string PublisherSession::localEndpointToString() const
{
    asio::error_code ec;
    auto local_endpoint = data_socket_.local_endpoint(ec);
    if (!ec)
        return local_endpoint.address().to_string() + ":" + std::to_string(local_endpoint.port());
    else
        return "?";
}

std::string PublisherSession::remoteEndpointToString() const
{
    asio::error_code ec;
    auto remote_endpoint = data_socket_.remote_endpoint(ec);
    if (!ec)
        return remote_endpoint.address().to_string() + ":" + std::to_string(remote_endpoint.port());
    else
        return "?";
}

std::string PublisherSession::endpointToString() const
{
    return localEndpointToString() + "->" + remoteEndpointToString();
}

} // namespace stps
