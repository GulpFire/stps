#pragma once

#include <functional>
#include <deque>

#include <asio.hpp>

#include "tcp_header.h"

namespace stps
{
	class PublisherSession : public std::enable_shared_from_this<PublisherSession>
	{
		private:
			enum class State
			{
				NotStarted,
				HandShaking,
				Running,
				Canceled
			};

		public:
			PublisherSession(const std::shared_ptr<asio::io_service>& io_service,
					const std::function<void(const std::shared_ptr<PublisherSession>&)>& session_closed_handler);

			PublisherSession(const PublisherSession&) = delete;
			
			PublisherSession& operator=(const PublisherSession&) = delete;

			PublisherSession& operator=(PublisherSession&&) = delete;

			PublisherSession(PublisherSession&&) = delete;

			~PublisherSession();

			void start();

			void cancel();

			void sendDataBuffer(const std::shared_ptr<std::vector<char>>& buf);

			asio::ip::tcp::socket& getSocket();

			std::string localEndpointToString() const;
			
			std::string remoteEndpointToString() const;

			std::string endpointToString() const;

		private:
			std::shared_ptr<asio::io_service> io_service_;
			std::atomic<State> state_;
			const std::function<void(const std::shared_ptr<PublisherSession>&)> session_closed_handler_;
			asio::ip::tcp::socket data_socket_;
			asio::io_service::strand data_strand_;
			std::mutex next_buffer_mutex_;
			bool sending_in_progress_;
			std::shared_ptr<std::vector<char>> next_buffer_to_send_;

			void sessionCloseHandler();

			void receiveTcpPacket();
			
			void readHeaderLength();

			void discardDataBetweenHeaderAndPayload(const std::shared_ptr<TcpHeader>& header, 
					uint16_t bytes_to_discard);

			void readHeaderContent(const std::shared_ptr<TcpHeader>& header);

			void readPayload(const std::shared_ptr<TcpHeader>& header);

			void sendProtocolHandshakeResponse();

			void sendBufferToClient(const std::shared_ptr<std::vector<char>>& buf);
	}
} // namespace stps
