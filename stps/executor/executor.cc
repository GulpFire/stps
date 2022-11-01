#include <stps/executor.h>
#include <stps/executor_impl.h>

namespace stps {

Executor::Executor(size_t thread_count)
	: executor_impl_()
{
	executor_impl_.start(thread_count);
}

Executor::~Executor()
{
	executor_impl_->stop();
}
} // namespace stps
