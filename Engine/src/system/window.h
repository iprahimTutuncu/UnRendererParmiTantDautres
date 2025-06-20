#pragma once

#include "../graphics/graphic_api.h"

#include <functional>
#include <memory>
#include <vector>

namespace GTS {
    class Event;

    struct GpuHandle {
        void* ptr = nullptr;

        template <typename T>
        T* as() const {
            return static_cast<T*>(ptr);
        }
        bool valid() const {
            return ptr != nullptr;
        }
    };

    struct WindowHandle {
        void* ptr = nullptr;

        template <typename T>
        T* as() const {
            return static_cast<T*>(ptr);
        }
        bool valid() const {
            return ptr != nullptr;
        }
    };

    enum class WindowAPI {
        None,
        SDL3
    };

    class Window {
    public:
        Window() = default;
        virtual ~Window() = default;

        virtual bool init(const int width, const int height, const char* title) = 0;
        virtual void setTitle(const char* title) = 0;
        virtual void setSize(const int width, const int height) = 0;
        virtual void close() = 0;
        virtual bool isRunning() = 0;
        virtual WindowHandle getWindow() = 0;
        virtual void enableEventForHUD() {
            isEventEnableForHUD = true;
        }
        virtual void disableEventForHUD() {
            isEventEnableForHUD = false;
        }
        virtual std::vector<Event> pollEvent() = 0;
        virtual void setResizeCallback(std::function<void(int, int)> callback) = 0;
        virtual GpuHandle getGpuDevice() = 0;

        static std::shared_ptr<Window> create(WindowAPI windowAPI);

    protected:
        int width { 0 };
        int height { 0 };
        bool running { true };
        bool isEventEnableForHUD { false };
        const char* title { nullptr };
        GraphicAPI graphicAPI { GraphicAPI::None };
        WindowAPI windowAPI { WindowAPI::None };
    };
}
