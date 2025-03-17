#include "pch.h"
#include "system/system_manager.h"
#include "system/log_manager.h"
#include "system/window.h"

namespace Thsan
{
	void Thsan::SystemManager::init(std::shared_ptr<ControlSetting> controlSetting)
	{
		LogManager::init();

		pWindow = Window::create(WindowAPI::SDL3);

		if (!pWindow->init(800, 600, "Thsan Engine"))
		{
			return;
		}

		pWindow->enableEventForHUD();

	}

	void Thsan::SystemManager::update()
	{
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