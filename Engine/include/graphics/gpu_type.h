#pragma once

#include <string>
#include <cstdint>
#include <iostream>
#include <vulkan/vulkan.h>

using namespace std;
#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			std::cout <<"Detected Vulkan error: " << err << std::endl; \
			abort();                                                \
		}                                                           \
	} while (0)

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;

using f32 = float;
using f64 = double;

namespace Thsan
{
    namespace QueueType
    {
        enum class Enum 
        {
            Graphics,
            Compute,
            Transfer
        };

        std::string toString(Enum type);
        Enum fromString(const std::string& str);
    }

    namespace TopologyType
    {
        enum class Enum
        {
            Points,
            Lines,
            Triangles
        };

        std::string toString(Enum type);
        Enum fromString(const std::string& str);
    }

}

