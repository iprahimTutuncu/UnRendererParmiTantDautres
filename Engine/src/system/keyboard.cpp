#include "pch.h"
#include "system/keyboard.h"
#include "system/log.h"

#include <SDL3/SDL_keyboard.h>

namespace Thsan
{
	std::array<bool, Keyboard::keyCount> Keyboard::keys;
	std::array<bool, Keyboard::keyCount> Keyboard::keysDown;
	std::array<bool, Keyboard::keyCount> Keyboard::keysUp;
	
	void Keyboard::init()
	{
		std::fill(keys.begin(), keys.end(), false);
		std::fill(keysDown.begin(), keysDown.end(), false);
		std::fill(keysUp.begin(), keysUp.end(), false);
	}

	void Keyboard::update()
	{
		std::fill(keysDown.begin(), keysDown.end(), false);
		std::fill(keysUp.begin(), keysUp.end(), false);

		const bool* state = SDL_GetKeyboardState(nullptr); // does not exist in sdl3

		for (int i = Key::kFirst; i < keyCount; i++)
		{
			bool wasDown = keys[i];
			keys[i] = state[i];
			bool isDown = keys[i];

			if (wasDown && !isDown)
				keysUp[i] = true;
			else if (!wasDown && isDown)
				keysDown[i] = true;
		}
	}

	bool Keyboard::key(Key keyVal)
	{
		TS_ASSERT(keyVal >= Key::kFirst && keyVal < keyCount, "Invalid keyboard key!");
		if (keyVal >= Key::kFirst && keyVal < keyCount)
			return keys[keyVal];

		return false;
	}

	bool Keyboard::keyDown(Key keyVal)
	{
		TS_ASSERT(keyVal >= Key::kFirst && keyVal < keyCount, "Invalid keyboard key!");
		if (keyVal >= Key::kFirst && keyVal < keyCount)
			return keysDown[keyVal];

		return false;
	}

	bool Keyboard::keyUp(Key keyVal)
	{
		TS_ASSERT(keyVal >= Key::kFirst && keyVal < keyCount, "Invalid keyboard key!");
		if (keyVal >= Key::kFirst && keyVal < keyCount)
			return keysUp[keyVal];

		return false;
	}

	bool is_key_pressed(Key keyVal)
	{
		return Keyboard::key(keyVal);
	}
	bool is_key_pressed_no_repeat(Key keyVal)
	{
		return Keyboard::keyDown(keyVal);
	}
	bool is_key_released(Key keyVal)
	{
		return  Keyboard::keyUp(keyVal);
	}
}