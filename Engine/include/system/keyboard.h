#pragma once
#include "key_code.h"
#include <array>

namespace Thsan {


    class Keyboard {
    public:
        static void init();
        static void update();

        static bool key(Key keyVal);
        static bool keyDown(Key keyVal);
        static bool keyUp(Key keyVal);
    private:
        constexpr static const int keyCount = 291; //SDL support up to index 290

        static std::array<bool, keyCount> keys;
        static std::array<bool, keyCount> keysDown;
        static std::array<bool, keyCount> keysUp;
    };

    bool is_key_pressed(Key keyVal);
    bool is_key_pressed_no_repeat(Key keyVal);
    bool is_key_released(Key keyVal);

}
