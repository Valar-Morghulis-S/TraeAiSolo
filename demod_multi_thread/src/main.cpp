#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <cstring>
#include <getopt.h>
#include <libgen.h>
#include "thread_pool.h"
#include "demod_wrapper.h"

struct DDCHeader {
    uint32_t magic;        // Header magic
    uint32_t channel;      // Channel number (0-255)
    uint32_t dataLen;      // Data length following header
    uint8_t  reserved[116]; // Reserved/padding
};

static const uint32_t DDC_MAGIC = 0xDDCCBBAA;
static const size_t HEADER_SIZE = 128;
static const size_t MAX_CHANNELS = 256;

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options] <input_file>\n"
              << "Options:\n"
              << "  -t, --threads N    Number of threads in pool (default: 8)\n"
              << "  -n, --num-demod N  Number of demodulator instances (default: 256)\n"
              << "  -h, --help         Show this help message\n"
              << "\n"
              << "Example:\n"
              << "  " << progName << " -t 8 -n 256 test.iq\n";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    int numThreads = 8;
    int numDemod = 256;
    std::string inputFile;

    static struct option longOptions[] = {
        {"threads",  required_argument, 0, 't'},
        {"num-demod", required_argument, 0, 'n'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int optionIndex = 0;
    while ((opt = getopt_long(argc, argv, "t:n:h", longOptions, &optionIndex)) != -1) {
        switch (opt) {
            case 't':
                numThreads = std::atoi(optarg);
                break;
            case 'n':
                numDemod = std::atoi(optarg);
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }

    if (optind < argc) {
        inputFile = argv[optind];
    } else {
        // Create a default test file if no input specified
        inputFile = "test_data.iq";
        std::cout << "No input file specified, creating test file: " << inputFile << std::endl;
    }

    std::cout << "=== Demod Multi-Thread Demo ===" << std::endl;
    std::cout << "Thread pool size: " << numThreads << std::endl;
    std::cout << "Number of demodulators: " << numDemod << std::endl;
    std::cout << "Input file: " << inputFile << std::endl;
    std::cout << std::endl;

    // Generate test data if file doesn't exist
    {
        std::ifstream testFile(inputFile, std::ios::binary);
        if (!testFile.good()) {
            std::cout << "Generating test data file..." << std::endl;
            std::ofstream outFile(inputFile, std::ios::binary);
            if (!outFile) {
                std::cerr << "Failed to create test file: " << inputFile << std::endl;
                return 1;
            }

            // Generate simulated DDC data
            const size_t numBlocks = 1000;
            const size_t blockSize = 256;  // 128 bytes header + 128 bytes data

            for (size_t block = 0; block < numBlocks; ++block) {
                uint8_t buffer[blockSize] = {0};

                // Fill header
                DDCHeader* header = reinterpret_cast<DDCHeader*>(buffer);
                header->magic = DDC_MAGIC;
                header->channel = block % MAX_CHANNELS;  // Distribute across 256 channels
                header->dataLen = blockSize - HEADER_SIZE;

                // Fill data
                for (size_t i = HEADER_SIZE; i < blockSize; ++i) {
                    buffer[i] = static_cast<uint8_t>((i * block) & 0xFF);
                }

                outFile.write(reinterpret_cast<char*>(buffer), blockSize);
            }
            outFile.close();
            std::cout << "Generated " << numBlocks << " blocks of test data" << std::endl;
        }
    }

    // Open input file
    std::ifstream infile(inputFile, std::ios::binary | std::ios::ate);
    if (!infile) {
        std::cerr << "Failed to open input file: " << inputFile << std::endl;
        return 1;
    }

    size_t fileSize = infile.tellg();
    infile.seekg(0, std::ios::beg);

    std::cout << "File size: " << fileSize << " bytes" << std::endl;
    std::cout << "Estimated blocks: " << (fileSize / (HEADER_SIZE + 128)) << std::endl;
    std::cout << std::endl;

    // Create thread pool
    ThreadPool pool(numThreads);
    std::cout << "Created thread pool with " << pool.getThreadCount() << " threads" << std::endl;

    // Create demod wrapper with 256 instances
    DemodWrapper demodWrap(numDemod, pool);
    if (!demodWrap.init()) {
        std::cerr << "Failed to initialize demod wrappers" << std::endl;
        return 1;
    }
    std::cout << "Initialized " << numDemod << " demodulator instances" << std::endl;

    // Read and process file
    std::cout << "\nProcessing file..." << std::endl;
    auto startTime = std::chrono::high_resolution_clock::now();

    const size_t blockDataSize = 128;
    const size_t totalBlockSize = HEADER_SIZE + blockDataSize;
    std::vector<uint8_t> buffer(totalBlockSize);
    uint64_t processedBlocks = 0;
    uint64_t totalBytes = 0;

    while (infile.read(reinterpret_cast<char*>(buffer.data()), totalBlockSize)) {
        DDCHeader* header = reinterpret_cast<DDCHeader*>(buffer.data());

        // Verify header
        if (header->magic == DDC_MAGIC && header->channel < MAX_CHANNELS) {
            // Submit processing task to thread pool
            uint32_t channel = header->channel;
            std::vector<uint8_t> dataCopy(blockDataSize);
            std::memcpy(dataCopy.data(), buffer.data() + HEADER_SIZE, blockDataSize);

            int threadId = channel % numThreads;
            demodWrap.processBlock(channel, dataCopy.data(), dataCopy.size(), threadId);

            processedBlocks++;
            totalBytes += blockDataSize;
        }
    }

    // Handle any remaining data
    if (infile.gcount() > 0) {
        std::cerr << "Warning: Incomplete block at end of file (" << infile.gcount() << " bytes)" << std::endl;
    }

    infile.close();

    // Wait for all tasks to complete
    std::cout << "Waiting for tasks to complete..." << std::endl;
    pool.waitAll();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "\n=== Processing Complete ===" << std::endl;
    std::cout << "Processed blocks: " << processedBlocks << std::endl;
    std::cout << "Total bytes: " << totalBytes << std::endl;
    std::cout << "Time elapsed: " << duration.count() << " ms" << std::endl;
    if (duration.count() > 0) {
        std::cout << "Throughput: " << (totalBytes * 1000.0 / duration.count() / 1024.0) << " KB/s" << std::endl;
    }

    // Performance metrics
    std::cout << "\n=== Performance Metrics ===" << std::endl;
    std::cout << "Thread pool threads: " << numThreads << std::endl;
    std::cout << "Demod instances: " << numDemod << std::endl;
    std::cout << "Blocks per second: " << (processedBlocks * 1000.0 / duration.count()) << std::endl;

    std::cout << "\nOutput files created:" << std::endl;
    for (int i = 0; i < numThreads; ++i) {
        std::string filename = "thread_" + std::to_string(i) + "_output.bin";
        std::ifstream test(filename, std::ios::binary);
        if (test) {
            test.seekg(0, std::ios::end);
            std::cout << "  " << filename << " (" << test.tellg() << " bytes)" << std::endl;
        }
    }

    return 0;
}