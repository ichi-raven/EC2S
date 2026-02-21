/*****************************************************************//**
 * @file   EC2S.hpp
 * @brief  header file of whole EC2S
 * @details if you want to use EC2S, just include this header file
 * 
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/

// 

// if you want to check hash synonym, use below macro (it will cause a runtime overhead, so not recommended in release build)
//#define EC2S_CHECK_SYNONYM
#include "Registry.hpp"

// optional
#include "Application.hpp"
#include "JobSystem.hpp"
#include "TLSFAllocator.hpp"
#include "ArenaAllocator.hpp"
