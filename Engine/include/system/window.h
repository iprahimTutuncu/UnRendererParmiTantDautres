#pragma once

#include "graphics/graphic_api.h"
#include "graphics/gpu_device.h"

#include <vector>
#include <functional>

namespace  Thsan
{
	class Event;

	class Window
	{
	public:
		Window() = default;
		virtual ~Window() = default;

		virtual bool init(const int width, const int height, const char* title) = 0;
		virtual void setTitle(const char* title) = 0;
		virtual void setSize(const int width, const int height) = 0;
		virtual void close() = 0;
		virtual bool isRunning() = 0;
		virtual void swapBuffers() = 0;
		virtual void enableEventForHUD() { isEventEnableForHUD = true; }
		virtual void disableEventForHUD() { isEventEnableForHUD = false; }
		virtual std::vector<Event> pollEvent() = 0;
		virtual void setResizeCallback(std::function<void(int, int)> callback) = 0;
		virtual std::shared_ptr<GpuDevice> getGpuDevice() = 0;

		static std::shared_ptr<Window> create(WindowAPI windowAPI);

	protected:

		int width{ 0 };
		int height{ 0 };
		bool running{ true };
		bool isEventEnableForHUD{ false };
		const char* title{ nullptr };
		GraphicAPI graphicAPI{ GraphicAPI::None };
		WindowAPI windowAPI{ WindowAPI::None };
	};
}
