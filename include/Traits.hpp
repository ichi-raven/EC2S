/*****************************************************************//**
 * @file   Traits.hpp
 * @brief  �K�v��Trait���`����w�b�_�t�@�C��
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
