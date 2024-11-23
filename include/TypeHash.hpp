/*****************************************************************/ /**
 * @file   TypeHash.hpp
 * @brief  header file of TypeHashGenerator class and compiler-dependent macros
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_TYPEHASH_HPP_
#define EC2S_TYPEHASH_HPP_

#include <cstdint>
#include <string_view>
#include <vector>
#include <utility>

#include <cassert>

#ifdef EC2S_CHECK_SYNONYM
#include <unordered_map>
#endif

//! macros to identify execution environment OS
#if defined _WIN32 || defined __CYGWIN__ || defined _MSC_VER
#define GENERATOR_EXPORT __declspec(dllexport)
#define GENERATOR_IMPORT __declspec(dllimport)
#elif defined __GNUC__ && __GNUC__ >= 4
#define GENERATOR_EXPORT __attribute__((visibility("default")))
#define GENERATOR_IMPORT __attribute__((visibility("default")))
#else /* Unsupported compiler */
#define GENERATOR_EXPORT
#define GENERATOR_IMPORT
#endif

//! macros to identify DLL import/export
#ifndef GENERATOR_API
#if defined GENERATOR_API_EXPORT
#define GENERATOR_API GENERATOR_EXPORT
#elif defined GENERATOR_API_IMPORT
#define GENERATOR_API GENERATOR_IMPORT
#else /* No API */
#define GENERATOR_API
#endif
#endif

//! macros to identify execution environment compiler (MSVC, gcc, clang)
#if defined _MSC_VER
#define GENERATOR_PRETTY_FUNCTION __FUNCSIG__
#elif defined __clang__ || (defined __GNUC__)
#define GENERATOR_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

namespace ec2s
{
    //! TypeHash type declaration
    using TypeHash = std::size_t;

    /** 
     * @brief  compile-time hash function (famous implementation)
     *  
     * @param s input string
     * @param count string s's index
     * @return hash value calculated from input string s
     */
    consteval std::uint32_t fnv1a_32(char const* s, const std::size_t count)
    {
        return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
    }

    /**
     * @brief  class to generate type hash at compile time
     * @detail realized by hashing the distinguished name of the function expanded with the template argument \
     *         if compiled with the EC2S_CHECK_SYNONYM macro defined, it is changed to a runtime type hash and the synonyms in the type range being called are checked by unordered_map
     */
    struct TypeHasher
    {
#ifdef EC2S_CHECK_SYNONYM

        /**
         * @brief  obtain a hash of the specified type (synonym check, hashing function name ver)
         * 
         * @tparam Type seeking hash
         */
#ifdef GENERATOR_PRETTY_FUNCTION
        template <typename Type>
        static std::size_t hash()
        {
            constexpr std::string_view str(GENERATOR_PRETTY_FUNCTION);
            constexpr std::size_t hashVal = static_cast<std::size_t>(fnv1a_32(str.data(), str.size()));

            auto&& itrPair = hashHistory.try_emplace(hashVal, str);
            if (!itrPair.second && itrPair.first->second != str)
            {
                assert(!"type hash synonim!");
            }

            return hashVal;
        }
#else
        /**
         * @brief  obtain a hash of the specified type (synonym check, simple ID ver)
         * 
         * @tparam Type seeking hash
         */
        template <typename Type>
        static std::size_t hash()
        {
            static const std::size_t value = TypeIDGenerator<Type>::next();
            return value;
        }
#endif
#else
#ifdef GENERATOR_PRETTY_FUNCTION
        /**
         * @brief  obtain a hash of the specified type
         * 
         * @tparam Type seeking hash
         */
        template <typename Type>
        static consteval std::size_t hash()
        {
            constexpr std::string_view str(GENERATOR_PRETTY_FUNCTION);
            constexpr std::size_t hashVal = static_cast<std::size_t>(fnv1a_32(str.data(), str.size()));

            return hashVal;
        }
#else
        /**
         * @brief  obtain a hash of the specified type (simple ID ver)
         * 
         * @tparam Type seeking hash
         */
        template <typename Type>
        static std::size_t hash()
        {
            static const std::size_t value = TypeIDGenerator<Type>::next();
            return value;
        }
#endif
#endif

    private:
        /**
         * @brief  type hash computation types in environments where unique function names are not available
         */
        struct GENERATOR_API TypeIDGenerator
        {
            /** 
             * @brief  return a unique unsigned integer value for each type
             *  
             * @return unique unsigned integer value
             */
            static std::size_t next()
            {
                static std::size_t value{};
                return value++;
            }
        };

#ifdef EC2S_CHECK_SYNONYM
        //! for synonym detection
        inline static std::unordered_map<std::size_t, std::string_view> hashHistory = std::unordered_map<std::size_t, std::string_view>();
#endif
    };

}  // namespace ec2s

#endif
