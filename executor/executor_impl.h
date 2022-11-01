#pragma once

#include <stdint.h>

#include <thread>
#include <string>
#include <vector>
#include <memory>

#include <asio.hpp>

namespace stps
{
	class ExecutorImpl : public std::enable_shared_from_this<ExecutorImpl>
	{
		public:
			ExecutorImpl();
			~ExecutorImpl();
			
			ExecutorImpl(const ExecutorImpl&) = delete;

			ExecutorImpl& operator=(const ExecutorImpl&) = delete;

			ExecutorImpl& operator=(ExecutorImpl&&) = default;

			ExecutorImpl(ExecutorImpl&&) = default;

			void start(size_t thread_count);
			
			void stop();

			std::shared_ptr<asio::io_service> ioService() const;
		
		private:
			std::shared_ptr<asio::io_service>	io_service_;
			std::vector<std::thread> thread_pool_;
			std::shared_ptr<asio::io_service::work> dummy_work_;
	};
} // namespace stps
