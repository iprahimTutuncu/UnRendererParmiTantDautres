#pragma once

#include <functional>
#include <unordered_map>

using action_t = std::function<void()>;

class ActionManager {

public:
    ActionManager() = default;
    ~ActionManager() = default;

private:
    std::unordered_map<unsigned int, action_t> _bindings;

public:
    void set_action(unsigned int key, action_t func);
    void do_action(unsigned int key) const;
};
