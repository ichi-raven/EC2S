
#ifndef CECS_ENTITY_HPP_
#define CECS_ENTITY_HPP_
#include <cstdint>

namespace ec2s
{
	using Entity = std::uint64_t;
	constexpr std::uint64_t kEntitySlotMask  = 0xFFFFFFFF00000000;
	constexpr std::uint64_t kEntityIndexMask = 0x00000000FFFFFFFF;
	constexpr std::uint64_t kEntitySlotShiftWidth = sizeof(Entity) * 4;
}

#endif
