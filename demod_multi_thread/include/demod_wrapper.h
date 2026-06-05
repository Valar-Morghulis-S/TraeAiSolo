#ifndef DEMOD_WRAPPER_H
#define DEMOD_WRAPPER_H

#include "demod_api.h"
#include "thread_pool.h"
#include <vector>
#include <memory>
#include <string>
#include <fstream>
#include <mutex>

struct DemodOutput;

class DemodWrapper {
public:
    DemodWrapper(size_t numInstances, ThreadPool& pool);
    ~DemodWrapper();

    // Initialize all demodulator instances
    bool init();

    // Process data block for specific channel
    void processBlock(uint32_t channel, const uint8_t* data, size_t dataLen, int threadId);

    // Get thread pool reference
    ThreadPool& getThreadPool() { return threadPool_; }

    // Write output data (called from callback)
    void writeOutput(int threadId, const DemodOutput* output);

private:
    struct DemodInstance {
        DemodHandle handle;
        int threadId;
    };

    size_t numInstances_;
    std::vector<std::unique_ptr<DemodInstance>> instances_;
    ThreadPool& threadPool_;
    std::mutex file_mutex_;
    std::vector<std::ofstream*> outputFiles_;
};

#endif // DEMOD_WRAPPER_H