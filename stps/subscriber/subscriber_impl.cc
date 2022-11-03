#include <stps/subscriber/subscriber_impl.h>

#include <stps/tcp_header.h>
#include <endian.h>
#include <stps/subscriber/subscriber_session_impl.h>
#include <stps/executor/executor_impl.h>

namespace stps
{
  SubscriberImpl::SubscriberImpl(const std::shared_ptr<Executor>& executor)
    : executor_                    (executor)
    , user_callback_is_synchronous_(true)
    , synchronous_user_callback_   ([](const auto&){})
    , callback_thread_stop_        (true)
  {}

  SubscriberImpl::~SubscriberImpl()
  {
  }

  std::shared_ptr<SubscriberSession> SubscriberImpl::addSession(const std::string& address, uint16_t port, int max_reconnection_attempts)
  {

    std::function<std::shared_ptr<std::vector<char>>()> get_free_buffer_handler
            = [me = shared_from_this()]() -> std::shared_ptr<std::vector<char>>
              {
                return me->buffer_pool_.allocate();
              };

    std::function<void(const std::shared_ptr<SubscriberSessionImpl>&)> subscriber_session_closed_handler
            = [me = shared_from_this()](const std::shared_ptr<SubscriberSessionImpl>& subscriber_session_impl) -> void
              {
                std::lock_guard<std::mutex>session_list_lock(me->session_list_mutex_);

                auto session_it = std::find_if(me->session_list_.begin()
                                              , me->session_list_.end()
                                              , [&subscriber_session_impl] (const std::shared_ptr<SubscriberSession>& session) ->bool
                                                { return subscriber_session_impl == session->subscriber_session_impl_; });
                if (session_it != me->session_list_.end())
                {
                  me->session_list_.erase(session_it);
                }
                else
                {
                }
              };

    std::shared_ptr<SubscriberSession> subscriber_session(
       new SubscriberSession(std::make_shared<SubscriberSessionImpl>(executor_->executor_impl_->ioService()
                                                                    , address
                                                                    , port
                                                                    , max_reconnection_attempts
                                                                    , get_free_buffer_handler
                                                                    , subscriber_session_closed_handler)));

    setCallbackToSession(subscriber_session);

    {
      std::lock_guard<std::mutex> session_list_lock(session_list_mutex_);
      session_list_.push_back(subscriber_session);
      subscriber_session->subscriber_session_impl_->start();
    }

    return subscriber_session;
  }

  std::vector<std::shared_ptr<SubscriberSession>> SubscriberImpl::getSessions() const
  {
    std::lock_guard<std::mutex> session_list_lock(session_list_mutex_);
    return session_list_;
  }

  void SubscriberImpl::setCallback(const std::function<void(const CallbackData& callback_data)>& callback_function, bool synchronous_execution)
  {

    if (callback_thread_)
    {
      callback_thread_stop_ = true;
      last_callback_data_cv_.notify_all();

      if (std::this_thread::get_id() == callback_thread_->get_id())
        callback_thread_->detach();
      else
        callback_thread_->join();

      callback_thread_.reset();
    }

    const bool renew_synchronous_callbacks = (synchronous_execution || user_callback_is_synchronous_);

    if (synchronous_execution)
    {
      synchronous_user_callback_    = callback_function;
      user_callback_is_synchronous_ = synchronous_execution;

      std::unique_lock<std::mutex> callback_lock(last_callback_data_mutex_);
      last_callback_data_ = CallbackData();
    }
    if (!synchronous_execution)
    {
      synchronous_user_callback_    = [](const auto&) {};
      user_callback_is_synchronous_ = synchronous_execution;

      callback_thread_stop_ = false;
      callback_thread_ = std::make_unique<std::thread>(
                    [me = shared_from_this(), callback_function]()
                    {
                      for (;;)
                      {
                        CallbackData this_callback_data; 

                        {
                          std::unique_lock<std::mutex> callback_lock(me->last_callback_data_mutex_);
                          me->last_callback_data_cv_.wait(callback_lock, [&me]() -> bool { return bool(me->last_callback_data_.buffer_) || me->callback_thread_stop_; });

                          if (me->callback_thread_stop_) return;

                          std::swap(this_callback_data, me->last_callback_data_);                         }

                        callback_function(this_callback_data);
                      }
                    });
    }

    if (renew_synchronous_callbacks)
    {
      std::lock_guard<std::mutex> session_list_lock(session_list_mutex_);
      for (const auto& session : session_list_)
      {
        setCallbackToSession(session);
      }
    }
  }

  void SubscriberImpl::setCallbackToSession(const std::shared_ptr<SubscriberSession>& session)
  {
    if (user_callback_is_synchronous_)
    {
      session->subscriber_session_impl_->setSynchronousCallback(
                [callback = synchronous_user_callback_, me = shared_from_this()](const std::shared_ptr<std::vector<char>>& buffer, const std::shared_ptr<TCPHeader>& /*header*/)->void
                {
                  std::lock_guard<std::mutex> callback_lock(me->last_callback_data_mutex_);
                  if (me->user_callback_is_synchronous_)
                  {
                    CallbackData callback_data;
                    callback_data.buffer_           = buffer;
                    callback(callback_data);
                  }
                });
    }
    else
    {
      session->subscriber_session_impl_->setSynchronousCallback(
                [me = shared_from_this()](const std::shared_ptr<std::vector<char>>& buffer, const std::shared_ptr<TCPHeader>& /*header*/)->void
                {
                  std::lock_guard<std::mutex> callback_lock(me->last_callback_data_mutex_);
                  if (!me->user_callback_is_synchronous_)
                  {
                    me->last_callback_data_.buffer_           = buffer;

                    me->last_callback_data_cv_.notify_all();
                  }
                });
    }
  }

  void SubscriberImpl::cancel()
  {

    {
      std::lock_guard<std::mutex> session_list_lock(session_list_mutex_);
      for (const auto& session : session_list_)
      {
        session->cancel();
      }
    }

    if (callback_thread_)
    {
      callback_thread_stop_ = true;
      last_callback_data_cv_.notify_all();

      if (std::this_thread::get_id() == callback_thread_->get_id())
        callback_thread_->detach();
      else
        callback_thread_->join();

      callback_thread_.reset();
    }

    synchronous_user_callback_    = [](const auto&){};
    user_callback_is_synchronous_ = true;
  }

  std::string SubscriberImpl::subscriberIdString() const
  {
    std::stringstream ss;
    ss << "0x" << std::hex << this;
    return ss.str();
  }
}
