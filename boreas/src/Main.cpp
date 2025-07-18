#include "core/Application.hpp"

int main() {
    Application application{};
    application.init();
    application.run();
    return EXIT_SUCCESS;
}
