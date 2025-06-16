#pragma once  
#include <olaf/system/key_code.h>  
#include <SDL3/SDL_events.h> // Include SDL_Event  

namespace Olaf {  
    class Event {  
    public:  
        struct KeyEvent {  
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
        SDL_Event sdlEvent; // Add this member to store SDL_Event  
    };  
}
