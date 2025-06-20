#pragma once

#include <cstdint>
#include <string>
#include <vector>

/*
    TODO: as of now this image class can only load RGBA8 image format
    depending on the evolution of our needs, we may need to extent this
    class to support different type of image. if that scenario happen,
    make an enum of format and be quite flexible.

    also, load, save and abillity to access pixel individually might be a useful feature.
*/
namespace GTS {
    class Image {
    public:
        Image() = default;
        ~Image() = default;

        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;

        Image(Image&& other) noexcept;
        Image& operator=(Image&& other) noexcept;

        bool loadFromFile(const std::string& filename, int desiredChannels = 4);

        // Getters
        int getWidth() const {
            return width;
        }
        int getHeight() const {
            return height;
        }
        int getChannels() const {
            return channels;
        }
        const uint8_t* getData() const {
            return pixels.data();
        }
        uint8_t* getData() {
            return pixels.data();
        }
        size_t getSize() const {
            return pixels.size();
        }

        bool isValid() const {
            return !pixels.empty();
        }

    private:
        std::vector<std::uint8_t> pixels;
        int width = 0;
        int height = 0;
        int channels = 0;
    };
}
