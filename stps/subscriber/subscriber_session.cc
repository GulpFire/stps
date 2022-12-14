#include <stps/subscriber/subscriber_session.h>
#include <stps/subscriber/subscriber_session_impl.h>

namespace stps
{
SubscriberSession::SubscriberSession(const std::shared_ptr<SubscriberSessionImpl>& impl)
    : subscriber_session_impl_(impl)
  {}

  SubscriberSession::~SubscriberSession()
  {
    subscriber_session_impl_->cancel();
  }

  std::string SubscriberSession::getAddress() const
    { return subscriber_session_impl_->getAddress(); }

  uint16_t    SubscriberSession::getPort()    const
    { return subscriber_session_impl_->getPort(); }

  void SubscriberSession::cancel()
    { subscriber_session_impl_->cancel(); }

  bool SubscriberSession::isConnected() const
    { return subscriber_session_impl_->isConnected(); }
} // namespace stps
