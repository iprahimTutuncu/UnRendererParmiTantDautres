#include "pch.h"
#include "graphics/gpu_type.h"
#include <system/log.h>

namespace Thsan
{
    namespace QueueType
    {
        std::string toString(Enum type)
        {
            switch (type)
            {
            case Enum::Graphics: return "Graphics";
            case Enum::Compute: return "Compute";
            case Enum::Transfer: return "Transfer";
            default: return "Unknown";
            }
        }

        Enum fromString(const std::string& str)
        {
            static const std::unordered_map<std::string, Enum> lookup = {
                {"Graphics", Enum::Graphics},
                {"Compute", Enum::Compute},
                {"Transfer", Enum::Transfer}
            };

            auto it = lookup.find(str);
            if (it != lookup.end())
            {
                return it->second;
            }

            TS_ERROR("in fromString(const std::string& str), str doesn't map to any enum, defaulting to Enum::Graphics");
            return Enum::Graphics;
        }
    }

    namespace TopologyType
    {
        std::string toString(Enum type)
        {
            switch (type)
            {
            case Enum::Points: return "Points";
            case Enum::Lines: return "Lines";
            case Enum::Triangles: return "Triangles";
            default: return "Unknown";
            }
        }

        Enum fromString(const std::string& str)
        {
            static const std::unordered_map<std::string, Enum> lookup = {
                {"Points", Enum::Points},
                {"Lines", Enum::Lines},
                {"Triangles", Enum::Triangles}
            };

            auto it = lookup.find(str);
            if (it != lookup.end())
            {
                return it->second;
            }

            TS_ERROR("in fromString(const std::string& str), str doesn't map to any enum, defaulting to Enum::Triangles");
            return Enum::Triangles;
        }
    }
}
