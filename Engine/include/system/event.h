#pragma once
#include "system/key_code.h"

namespace Thsan 
{
	class Event 
	{
	public:
		struct KeyEvent 
		{
			Key code;
		};

		union {
			KeyEvent key;
		};

		enum EventType {
			None = 0,
			KeyPressed = 1,
			KeyReleased = 2
		};

		EventType type;
	};
}