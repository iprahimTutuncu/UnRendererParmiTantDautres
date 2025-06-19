#include "pch.h"
#include "options.h"
#include "system/system_manager.h"
#include "system/log_manager.h"
#include "system/window.h"
#include "system/event.h"

namespace GTS
{
	void GTS::SystemManager::init(WindowOptions& options, std::shared_ptr<ControlSetting> controlSetting, std::function<void(Options&, const double&, const std::vector<InputAction>&)> onInputCallback)
	{
		LogManager::init();

		pWindow = Window::create(WindowAPI::SDL3);

		if (!pWindow->init(options.screenWidth, options.screenHeight, "GTS Engine"))
		{
			return;
		}
		pControlSetting = controlSetting;
		this->onInput = onInputCallback;

		pWindow->enableEventForHUD();
	}

	void GTS::SystemManager::update(Options& options, double dt)
	{
		std::vector<Event> events = pWindow->pollEvent();

		for (Event e : events)
			pControlSetting->handleInput(e);

		pControlSetting->updateInput();

		if(onInput) onInput(options, dt, pControlSetting->getInput());
	}

	void SystemManager::close()
	{
		if(pWindow)
			pWindow->close();

		LogManager::shutdown();
	}


	std::shared_ptr<Window> SystemManager::getWindow()
	{
		return pWindow;
	}
}