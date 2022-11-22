/*****************************************************************//**
 * @file   Traits.hpp
 * @brief  必要なTraitを定義するヘッダファイル
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_TRAITS_HPP_
#define EC2S_TRAITS_HPP_

#include <type_traits>

namespace ec2s
{
	namespace Traits
	{
		template<typename Func, typename... Types>
		using IsEligibleEachFunc = std::enable_if_t<std::is_invocable_v<Func, Types&...>>;
	}
}

#endif
