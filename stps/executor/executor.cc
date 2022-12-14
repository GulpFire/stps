#include <stps/executor/executor.h>
#include <stps/executor/executor_impl.h>

namespace stps {

Executor::Executor(size_t thread_count)
	: executor_impl_(std::make_shared<ExecutorImpl>())
{
	executor_impl_->start(thread_count);
}

Executor::~Executor()
{
	executor_impl_->stop();
}
} // namespace stps
