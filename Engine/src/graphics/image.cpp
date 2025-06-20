#include "image.h"

#include "../system/log.h"

#include <stb_image.h>

namespace GTS {
    Image::Image(Image&& other) noexcept
        : pixels(std::move(other.pixels))
        , width(other.width)
        , height(other.height)
        , channels(other.channels) {
        other.width = 0;
        other.height = 0;
        other.channels = 0;
    }

    Image& Image::operator=(Image&& other) noexcept {
        if (this != &other) {
            pixels = std::move(other.pixels);
            width = other.width;
            height = other.height;
            channels = other.channels;

            other.width = 0;
            other.height = 0;
            other.channels = 0;
        }
        return *this;
    }

    bool Image::loadFromFile(const std::string& filename, int desiredChannels) {
        pixels.clear();

        uint8_t* tempData = stbi_load(filename.c_str(), &width, &height, &channels, desiredChannels);
        if (!tempData) {
            GTS_ERROR("Failed to load image: '{}' because of {}", filename, stbi_failure_reason()); // Optional: improve logging
            width = height = channels = 0;
            return false;
        }

        if (desiredChannels > 0)
            channels = desiredChannels;

        size_t size = static_cast<size_t>(width * height * channels);
        pixels.resize(size);
        std::memcpy(pixels.data(), tempData, size);

        stbi_image_free(tempData);
        return true;
    }
}
