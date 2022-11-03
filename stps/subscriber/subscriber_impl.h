#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <vector>
#include <functional>
#include <thread>
#include <condition_variable>
#include <asio.hpp>
#include <recycle/shared_pool.hpp>

#include <stps/executor/executor.h>
#include <stps/subscriber/subscriber_session.h>
#include <stps/callback_data.h>

namespace stps
{
class SubscriberImpl : public std::enable_shared_from_this<SubscriberImpl>
{
    public:
        SubscriberImpl(const std::shared_ptr<Executor>& executor);
        SubscriberImpl(const SubscriberImpl&)            = delete;
        SubscriberImpl& operator=(const SubscriberImpl&) = delete;

        SubscriberImpl& operator=(SubscriberImpl&&)      = delete;
        SubscriberImpl(SubscriberImpl&&)                 = delete;

        ~SubscriberImpl();
    public:
    std::shared_ptr<SubscriberSession> addSession(const std::string& address, uint16_t port, int max_reconnection_attempts);
    std::vector<std::shared_ptr<SubscriberSession>> getSessions() const;
    void setCallback(const std::function<void(const CallbackData& callback_data)>& callback_function,       bool synchronous_execution);
  private:
    void setCallbackToSession(const std::shared_ptr<SubscriberSession>& session);

  public:
    void cancel();

  private:
    std::string subscriberIdString() const;
private:
    const std::shared_ptr<Executor>                 executor_;                 

    mutable std::mutex                              session_list_mutex_;
    std::vector<std::shared_ptr<SubscriberSession>> session_list_;

    mutable std::mutex                              last_callback_data_mutex_;
    std::condition_variable                         last_callback_data_cv_;
    CallbackData                                    last_callback_data_;

    std::atomic<bool>                               user_callback_is_synchronous_;
    std::function<void(const CallbackData&)>        synchronous_user_callback_;

    std::unique_ptr<std::thread>                    callback_thread_;
    std::atomic<bool>                               callback_thread_stop_;

    struct BufferPoolLockPolicy
    {
      using mutex_type = std::mutex;
      using lock_type  = std::lock_guard<mutex_type>;
    };

    recycle::shared_pool<std::vector<char>, BufferPoolLockPolicy> buffer_pool_;                
};
} // namespace stps
