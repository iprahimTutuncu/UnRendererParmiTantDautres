#include "image_manager.h"

#include "../graphics/image.h"

#include <memory>
#include <unordered_map>

namespace Ressource {
    std::unordered_map<std::string, std::shared_ptr<GTS::Image>> ImageManager::m_images;

    std::shared_ptr<GTS::Image> ImageManager::get(const std::string& imageFilename, int desiredChannels) {
        auto it = m_images.find(imageFilename);
        if (it != m_images.end()) {
            return it->second;
        }

        auto image = std::make_shared<GTS::Image>();
        if (!image->loadFromFile(imageFilename, desiredChannels)) {
            return nullptr;
        }

        m_images[imageFilename] = image;
        return image;
    }

    void ImageManager::releaseUnused() {
        for (auto it = m_images.begin(); it != m_images.end();) {
            if (it->second.use_count() == 1) {
                it = m_images.erase(it);
            } else {
                ++it;
            }
        }
    }
}
