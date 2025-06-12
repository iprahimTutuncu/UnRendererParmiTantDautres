#pragma once

#include <map>
#include <functional>

using action_t = std::function<void()>;

class ActionManager {

    public:
        ActionManager() = default;
        ~ActionManager() = default;

    private:
        std::map<unsigned int, action_t> _bindings;

    public:
        void set_action(unsigned int key, action_t func);
        void do_action(unsigned int key) const;
};

