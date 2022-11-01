#pragma once

#include <stdint.h>

namespace stps
{
#pragma pack(push, 1)

struct ProtocolHandshakeMessage
{
    uint8_t protocol_version = 0;
};

#pragma pack(pop)
} // namespace stps
