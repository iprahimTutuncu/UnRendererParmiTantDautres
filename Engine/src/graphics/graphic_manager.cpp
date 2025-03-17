#include "pch.h"
#include "graphics/graphic_manager.h"
#include "graphics/gpu_device.h"
#include "system/window.h"

void Thsan::GraphicsManager::init(std::shared_ptr<Window> window)
{
	pWindow = window;
	pGpuDevice = window->getGpuDevice();
}

void Thsan::GraphicsManager::update()
{
	pGpuDevice->draw();
}

void Thsan::GraphicsManager::close()
{
}
