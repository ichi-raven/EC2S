/*****************************************************************//**
 * @file   TypeHash.hpp
 * @brief  TypeHashGeneratorクラス及び環境依存マクロのヘッダファイル
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_TYPEHASH_HPP_
#define EC2S_TYPEHASH_HPP_

#if defined _WIN32 || defined __CYGWIN__ || defined _MSC_VER
#    define GENERATOR_EXPORT __declspec(dllexport)
#    define GENERATOR_IMPORT __declspec(dllimport)
#elif defined __GNUC__ && __GNUC__ >= 4
#    define GENERATOR_EXPORT __attribute__((visibility("default")))
#    define GENERATOR_IMPORT __attribute__((visibility("default")))
#else /* Unsupported compiler */
#    define GENERATOR_EXPORT
#    define GENERATOR_IMPORT
#endif


#ifndef GENERATOR_API
#   if defined GENERATOR_API_EXPORT
#       define GENERATOR_API GENERATOR_EXPORT
#   elif defined GENERATOR_API_IMPORT
#       define GENERATOR_API GENERATOR_IMPORT
#   else /* No API */
#       define GENERATOR_API
#   endif
#endif

#if defined _MSC_VER
#   define GENERATOR_PRETTY_FUNCTION __FUNCSIG__
#elif defined __clang__ || (defined __GNUC__)
#   define GENERATOR_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

#include <cstdint>
#include <string_view>

namespace ec2s
{
    using TypeHash = std::size_t;

    constexpr std::uint32_t fnv1a_32(char const* s, std::size_t count)
    {
        return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
    }

    template<typename Type>
    struct TypeHashGenerator
    {
#ifdef GENERATOR_PRETTY_FUNCTION
        static constexpr std::size_t id() 
        {
            constexpr std::string_view str(GENERATOR_PRETTY_FUNCTION);
            constexpr auto value = fnv1a_32(str.data(), str.size());
            return value;
        }
#else
        static std::size_t id() 
        {
            static const std::size_t value = TypeHashGenerator::next();
            return value;
        }
#endif

    private:

        struct GENERATOR_API generator
        {
            static std::size_t next() 
            {
                static std::size_t value{};
                return value++;
            }
        };
    };
}

#endif
