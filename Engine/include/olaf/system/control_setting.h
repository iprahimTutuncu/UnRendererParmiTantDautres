#pragma once

#include <olaf/system/event.h>
#include <olaf/system/keyboard.h>

#include <map>
#include <vector>

namespace Olaf {
    enum InputState {
        isPressed,
        isPressedNoRepeat,
        isReleased,
        isDoubleClick,
        JoystickAxisUp,
        JoystickAxisDown,
        JoystickAxisRight,
        JoystickAxisLeft
    };

    enum InputAction {
        up,
        down,
        left,
        right,
        rotateLeft,
        rotateRight,
        start,
        select,
        run,
        stop_run,
        jump,
        back,
        action,
        item,
        shield,
        aim,
        stop_aim,
        plant,
        attack
    };

    class ControlSetting {
    public:
        ControlSetting() = default;
        ~ControlSetting() = default;
        void add(Key key, InputState inputState, InputAction inputAction);
        void remove(Key key, InputState inputState, InputAction inputAction);

        std::vector<InputAction> getInput();

    private:
        void handleInput(Event event); // create some way to pass event from game
        void updateInput();

        std::map<std::pair<Key, InputState>, std::vector<InputAction>> inputMap;
        std::vector<InputAction> values;
        // std::map<sf::Keyboard::Key, bool> keyReleasedCheker;
        std::map<std::pair<Key, InputState>, bool> inputActive;

        friend class SystemManager;
    };
}
