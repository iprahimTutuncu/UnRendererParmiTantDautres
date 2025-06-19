#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <glm/glm.hpp>

class Texture;
using TexturePtr = std::shared_ptr<Texture>;

namespace GTS
{
    // --- Sprite State ---
    struct Sprite
    {
        glm::vec3 position{ 0.0f };
        glm::vec2 scale{ 1.0f };
        float rotation = 0.0f;
        glm::vec4 color{ 1.0f };       // RGBA tint (default: white)
        glm::vec4 keyColor{ 0.0f };    // Color-key transparency (default: transparent black)
        bool flipX = false;
        bool flipY = false;
        bool visible = true;
    };

    class SpriteWorld
    {
    public:
        using SpriteID = uint32_t;
        static constexpr SpriteID INVALID_ID = 0;


        // --- Animation Definition ---
        struct Animation
        {
            std::string name;
            std::vector<glm::vec4> frames;  // UV rects (x,y,w,h)
            float frameDuration = 0.1f;
            bool loop = true;
            float speed = 1.0f;
        };

        // --- Core API ---
        SpriteID createSprite(TexturePtr texture);
        void destroySprite(SpriteID id);
        bool exists(SpriteID id) const;

        // --- State Access ---
        Sprite& getState(SpriteID id);
        const Sprite& getState(SpriteID id) const;
        TexturePtr getTexture(SpriteID id) const;

        // --- Key Color ---
        void setKeyColor(SpriteID id, const glm::vec4& color);
        glm::vec4 getKeyColor(SpriteID id) const;

        // --- Animation Control ---
        void addAnimation(SpriteID id, const std::string& name, Animation animation);
        void play(SpriteID id, const std::string& animation, bool restart = false);
        void stop(SpriteID id);
        void pause(SpriteID id);
        void resume(SpriteID id);
        bool isPlaying(SpriteID id) const;
        std::string getCurrentAnimation(SpriteID id) const;

        // --- Frame Control ---
        void setFrame(SpriteID id, size_t frameIndex);
        size_t getCurrentFrame(SpriteID id) const;
        size_t getFrameCount(SpriteID id) const;
        float getAnimationProgress(SpriteID id) const; // Returns 0-1 normalized progress

        // --- System Update ---
        void update(float deltaTime); // Updates all active animations

        // --- Batch Rendering ---
        void drawAll() const; // Renders all visible sprites in zOrder

    private:
        struct SpriteData
        {
            TexturePtr texture;
            Sprite state;
            std::unordered_map<std::string, Animation> animations;
            const Animation* currentAnim = nullptr;
            size_t currentFrame = 0;
            float frameTimer = 0.0f;
            bool isPlaying = false;
        };

        std::unordered_map<SpriteID, SpriteData> m_sprites;
        SpriteID m_nextID = 1; // ID counter

        // Private helpers
        SpriteData* getSpriteData(SpriteID id);
        const SpriteData* getSpriteData(SpriteID id) const;
    };
};