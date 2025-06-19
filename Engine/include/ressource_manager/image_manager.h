#pragma once

#include <unordered_map>
#include <memory>
#include <string>

namespace GTS
{
    class Image;
}

namespace Ressource
{
    class ImageManager
    {
    public:
        static std::shared_ptr<GTS::Image> get(const std::string& imageFilename, int desiredChannels = 4);
        static void releaseUnused();

    private:
        static std::unordered_map<std::string, std::shared_ptr<GTS::Image>> m_images;

        ImageManager() = delete;
    };
}