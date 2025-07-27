#include "core/Application.hpp"

#include <UTL/profiler.hpp>

int main() {
    UTL_PROFILER("Main Application Loop") {

        Application application {};
        UTL_PROFILER("Initialization") {
            application.init();
        }

        UTL_PROFILER("Run Application") {
            application.run();
        }
    }
    return EXIT_SUCCESS;
}
