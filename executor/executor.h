#pragma once

#include <string>
#include <memory>

#include <stdint.h>

namespace stps
{
	class ExecutorImpl;
	class PublisherImpl;
	class SubscriberImpl;

	class Executor
	{
		public:
			Executor(size_t thread_count);
			
			~Executor();
			
			Executor(const Executor&) = delete;

			Executor& operator=(const Executor&) = delete;

			Executor& operator=(Executor&&) = default;

			Executor(Executor&&) = default;

		private:
			friend ::stps::PublisherImpl;
			friend ::stps::SubscriberImpl;
			std::shared_ptr<ExecutorImpl> executor_impl_;
	};
} // namespace stps
