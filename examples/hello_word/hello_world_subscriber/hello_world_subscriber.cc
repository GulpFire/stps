#include <iostream>
#include <thread>

#include <stps/executor/executor.h>
#include <stps/subscriber/subscriber.h>

int main()
{
    std::shared_ptr<stps::Executor> executor
        = std::make_shared<stps::Executor>(4);

    stps::Subscriber hello_world_subscriber(executor);
    auto session1 = hello_world_subscriber.addSession("127.0.0.1", 1588);

    std::function<void(const stps::CallbackData& callback_data)>callback_function
        = [](const stps::CallbackData& callback_data) -> void
        {
            std::string temp_string_representation(callback_data.buffer_->data(), 
                    callback_data.buffer_->size());
            std::cout << "Received payload: " << temp_string_represetation << std::endl;
        };

    hello_world_subscriber.setCallback(callback_function);

    for (;;)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    return 0;
}
