#pragma once

#include <stdint.h>

namespace stps
{

enum class MessageContentType : uint8_t
{
	RegularPayload = 0,
	ProtocolHandShake = 1
};

#pragma pack(push,1)

struct TCPHeader
{
	uint16_t header_size = 0;
	MessageContentType = MessageContentType::RegularPayload;
	uint8_t reserved = 0;
	uint64_t data_size = 0;
};

#pragma pack(pop)

} // namespace stps
