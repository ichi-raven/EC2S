/*****************************************************************//**
 * @file   Traits.hpp
 * @brief  header file of trait definitions
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_TRAITS_HPP_
#define EC2S_TRAITS_HPP_

#include <type_traits>

namespace ec2s
{
	//! namespace for all traits
	namespace Traits
	{
		/**
		 * @brief  whether a Func is a callable function type with Types&... as an arguments
		 * 
		 * @tparam Func function type
		 * @tparam Types arguments types
		 */
		template<typename Func, typename... Types>
		using IsEligibleEachFunc = std::enable_if_t<std::is_invocable_v<Func, Types&...>>;
	}
}

#endif
