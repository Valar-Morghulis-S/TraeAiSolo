#include "demod_framework.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>

int main(int argc, char* argv[]) {
    size_t maxThreads = 8;
    size_t maxChannels = 256;

    if (argc > 1) maxThreads = std::atoi(argv[1]);
    if (argc > 2) maxChannels = std::atoi(argv[2]);

    std::cout << "=== Demod Multi-Thread Framework Demo ===" << std::endl;
    std::cout << "Max threads: " << maxThreads << std::endl;
    std::cout << "Max channels: " << maxChannels << std::endl;
    std::cout << std::endl;

    // Create and initialize manager with thread pool
    Manager manager(maxThreads, maxChannels);
    if (!manager.init()) {
        std::cerr << "Failed to initialize manager" << std::endl;
        return 1;
    }

    // Create read thread for shared memory
    ReadThread reader(manager);

    std::cout << "Starting processing..." << std::endl;
    auto startTime = std::chrono::high_resolution_clock::now();

    // Start reading from shared memory
    reader.start();

    // Wait for read thread to finish
    reader.stop();

    // Wait for all thread pool tasks to complete
    manager.waitAll();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << std::endl;
    std::cout << "=== Processing Complete ===" << std::endl;
    std::cout << "Channels created: " << manager.getChannelCount() << std::endl;
    std::cout << "Time elapsed: " << duration.count() << " ms" << std::endl;

    // Print output file sizes
    std::cout << std::endl;
    std::cout << "Output files:" << std::endl;
    for (size_t i = 0; i < maxThreads; ++i) {
        std::string filename = "thread_" + std::to_string(i) + "_output.bin";
        std::ifstream test(filename, std::ios::binary | std::ios::ate);
        if (test) {
            std::cout << "  " << filename << " (" << test.tellg() << " bytes)" << std::endl;
        }
    }

    return 0;
}