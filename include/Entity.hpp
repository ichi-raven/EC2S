/*****************************************************************//**
 * @file   Entity.hpp
 * @brief  header file of Entity definition and some constants
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_ENTITY_HPP_
#define EC2S_ENTITY_HPP_

#include <cstdint>
#include <limits>

namespace ec2s
{
	//! Entity, gen slot(32bit) | index of SparseSet(32bit)
	using Entity = std::uint64_t;

	//! For the Entity type, the shift width such that 1 is shifted to reach the slot part
	constexpr Entity kEntitySlotShiftWidth = sizeof(Entity) * 4;
	//! Bitmask to obtain the slot portion for values of type Entity
	constexpr Entity kEntitySlotMask  = std::numeric_limits<Entity>::max() << kEntitySlotShiftWidth;
	//! Bit mask to get the index part for the value of Entity type
	constexpr Entity kEntityIndexMask = ~kEntitySlotMask;	
	//! Invalid entity ID
    constexpr Entity kInvalidEntity = kEntitySlotMask | kEntityIndexMask;
}

#endif
