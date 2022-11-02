#include <stps/executor/executor_impl.h>
#include <iostream>

namespace stps
{
	ExecutorImpl::ExecutorImpl()
		: io_service_(std::make_shared<asio::io_service>())
		, dummy_work_(std::make_shared<asio::io_service::work>(*io_service_))
	{
	}

	ExecutorImpl::~ExecutorImpl()
	{
		std::stringstream ss;
		ss << std::this_thread::get_id();
		std::string thread_id = ss.str();
		std::cout << "Executor: Deleting from thread " << thread_id << std::endl;
		for (std::thread& thread : thread_pool_)
		{
			thread.detach();
		}

		thread_pool_.clear();
	}

	void ExecutorImpl::start(size_t thread_count)
	{
		for (size_t i = 0; i < thread_count; ++i)
		{
			thread_pool_.emplace_back(
					[me = shared_from_this()]()
					{
						std::stringstream ss;
						ss << std::this_thread::get_id();
						std::string thread_id = ss.str();
						std::cout << "Executor: IoService::Run() in thread " + thread_id << std::endl;
						me->io_service_->run();
					});
		}
	}

	void ExecutorImpl::stop()
	{
		dummy_work_.reset();
		io_service_->stop();
	}

	std::shared_ptr<asio::io_service> ExecutorImpl::ioService() const
	{
		return io_service_;
	}
} // namespace stps
