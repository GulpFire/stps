#pragma once

#include <stps/tcp_header.h>

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include <asio.hpp>

namespace stps
{
class SubscriberSessionImpl : public std::enabled_shared_from_this<SubscriberSessionImpl>
{
    public:
        SubscriberSessionImpl(const std::shared_ptr<asio::io_service>& io_service,
                uint16_t address, int port, 
                const std::function<std::shared_ptr<SubscriberSessionImpl>&>& session_closed_handler);

        SubscriberSessionImpl(const SubscriberSessionImpl&) = delete;
        SubscriberSessionImpl& operator=(const SubscriberSessionImpl&) = delete;
        SubscriberSessionImpl& operator=(SubscriberSessionImpl&&) = delete;
        SubscriberSessionImpl(SubscriberSessionImpl&&) = delete;

        void start();

        void setSynchronousCallback(const std::function<void(const std::shared_ptr<std::vector<char>>&, const std::shared_ptr<TCPHeader>&)>& callback);

        std::string getAddress() const;

        uint16_t getPort() const;

        void cancel();

        bool isConnected() const;

        std::string remoteEndpointToString() const;
        std::string localEndpointToString() const;
        std::string endpointToString() const;

    private:
        std::string address_;
        uint16_t port_;
        asio::ip::tcp::resolver resolver_;
        asio::ip::tcp::endpoint endpoint_;
        int max_reconnection_attempts_;
        int retries_left_;
        asio::steady_timer retry_timer_;
        std::atomic<bool> canceled_;

        asio::ip::tcp::socket data_socket_;
        asio::io_service::strand data_strand_;

        const std::function<std::shared_ptr<std::vector<char>>()> get_buffer_handler_;
        const std::function<void(const std::shared_ptr<SusbcriberSessionImpl>&)> session_closed_handler_;
        std::function<void(const std::shared_ptr<std::vector<char>>&, const std::shared_ptr<TCPHeader>&)> synchronous_callback_;

        void resolveEndpoint();

        void connectToEndPoint(const asio::ip::tcp::resolver::iterator& resolved_endpoints);

        void sendProtokolHandshakeRequest();

        void connectionFailedHandler();

        void readHeaderLength();

        void readHeaderContent(const std::shared_ptr<TCPHeader>& header);

        void discardDataBetweenHeaderAndPayload(const std::shared_ptr<TCPHeader>& header, uint16_t bytes_to_discard);

        void readPayload(const std::shared_ptr<TCPHeader>& header);




};

} // namespace stps
