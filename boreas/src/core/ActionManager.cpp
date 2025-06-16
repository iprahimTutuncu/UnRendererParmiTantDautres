#include "core/ActionManager.hpp"

void ActionManager::set_action(unsigned int key, action_t func)
{
    _bindings[key] = func;
}

void ActionManager::do_action(unsigned int key) const 
{
    if (auto it = _bindings.find(key); it != _bindings.end()) {
        it->second();
    }
}
