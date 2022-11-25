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
#include <limits>

namespace ec2s
{
	//! Entity�^�C����X���b�g(32bit) | SparseSet�ւ̃C���f�b�N�X(32bit)
	using Entity = std::uint64_t;

	//! Entity�^�ɑ΂��C1���V�t�g���ăX���b�g�����܂œ��B����悤�ȃV�t�g��
	constexpr Entity kEntitySlotShiftWidth = sizeof(Entity) * 4;
	//! Entity�^�̒l�ɑ΂��C�X���b�g�������擾����r�b�g�}�X�N
	constexpr Entity kEntitySlotMask  = std::numeric_limits<Entity>::max() << kEntitySlotShiftWidth;
	//! Entity�^�̒l�ɑ΂��C�C���f�b�N�X�������擾����r�b�g�}�X�N
	constexpr Entity kEntityIndexMask = ~kEntitySlotMask;	
}

#endif
