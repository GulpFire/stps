#include <iostream>
#include <thread>

#include <stps/executor/executor.h>
#include <stps/publisher/publisher.h>

int main()
{
    std::shared_ptr<stps::Executor> executor = 
        std::make_shared<stps::Executor>(6);

    int counter = 0;
    stps::Publisher hello_world_publisher(executor, 1588);

    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::string data_to_send = "Hello World" + std::to_string(++counter);
        auto now = std::chrono::steady_clock::now();

        std::cout << "Sending " << data_to_send << std::endl;
        hello_world_publisher.send(&data_to_send[0], data_to_send.size());
    }

    return 0;
}
