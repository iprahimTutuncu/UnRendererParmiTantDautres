#include <chrono>
#include <iostream>
#include <thread>
#include <x86intrin.h>

/**
 * @brief Helper function to estimate the TSC frequency by measuring the time taken for a sleep operation.
 *
 * @return int
 */
int main() {
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t c0 = __rdtsc();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    uint64_t c1 = __rdtsc();
    auto t1 = std::chrono::high_resolution_clock::now();

    double elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
    double freq = (c1 - c0) / elapsed_sec;

    std::cout << "Estimated TSC frequency: " << freq / 1e6 << " MHz\n";
}
