/*****************************************************************//**
 * @file   Entity.hpp
 * @brief  Entity型及びEntity用定数定義ヘッダファイル
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
	//! Entity型，世代スロット(32bit) | SparseSetへのインデックス(32bit)
	using Entity = std::uint64_t;

	//! Entity型に対し，1をシフトしてスロット部分まで到達するようなシフト幅
	constexpr Entity kEntitySlotShiftWidth = sizeof(Entity) * 4;
	//! Entity型の値に対し，スロット部分を取得するビットマスク
	constexpr Entity kEntitySlotMask  = std::numeric_limits<Entity>::max() << kEntitySlotShiftWidth;
	//! Entity型の値に対し，インデックス部分を取得するビットマスク
	constexpr Entity kEntityIndexMask = ~kEntitySlotMask;	
}

#endif
