/*****************************************************************//**
 * @file   Entity.hpp
 * @brief  Entity�^�y��Entity�p�萔��`�w�b�_�t�@�C��
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_ENTITY_HPP_
#define EC2S_ENTITY_HPP_
#include <cstdint>

namespace ec2s
{
	using Entity = std::uint64_t;
	constexpr std::uint64_t kEntitySlotMask  = 0xFFFFFFFF00000000;
	constexpr std::uint64_t kEntityIndexMask = ~kEntitySlotMask;
	constexpr std::uint64_t kEntitySlotShiftWidth = sizeof(Entity) * 4;
}

#endif
