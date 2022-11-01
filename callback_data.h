#pragma once

#include <vector>
#include <chrono>
#include <memory>
#include <stdint.h>

namespace stps
{
struct CallbackData
{
    std::shared_ptr<std::vector<char>> buffer_;
};
} // namespace stps
