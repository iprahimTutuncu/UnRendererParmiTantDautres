#include "core/Application.hpp"

#include <UTL/profiler.hpp>

int main() {
    Application application {};

    UTL_PROFILER("Initialization")
    application.init();

    application.run();
    return EXIT_SUCCESS;
}
