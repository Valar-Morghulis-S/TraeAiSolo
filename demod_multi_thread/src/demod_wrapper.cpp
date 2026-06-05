#include "demod_wrapper.h"
#include <iostream>
#include <cstring>
#include <atomic>

static std::atomic<uint32_t> g_output_counter{0};

// Global demod wrapper instance for callback routing
static DemodWrapper* g_demod_wrapper_instance = nullptr;

// Static callback function - routes to correct wrapper
extern "C" void demodStaticCallback(struct DemodOutput* output) {
    // In real impl, would route based on instance. For demo, write to thread 0
    if (g_demod_wrapper_instance) {
        g_demod_wrapper_instance->writeOutput(0, output);
    }
}

DemodWrapper::DemodWrapper(size_t numInstances, ThreadPool& pool)
    : numInstances_(numInstances), threadPool_(pool) {
}

DemodWrapper::~DemodWrapper() {
    for (auto& inst : instances_) {
        if (inst && inst->handle) {
            Demod_Destroy(inst->handle);
        }
    }
    for (auto& f : outputFiles_) {
        if (f) {
            f->close();
            delete f;
        }
    }
}

bool DemodWrapper::init() {
    // Set global instance for callback routing
    g_demod_wrapper_instance = this;

    // Create output files for each thread
    size_t threadCount = threadPool_.getThreadCount();
    outputFiles_.resize(threadCount, nullptr);

    for (size_t i = 0; i < threadCount; ++i) {
        std::string filename = "thread_" + std::to_string(i) + "_output.bin";
        outputFiles_[i] = new std::ofstream(filename, std::ios::binary | std::ios::out);
        if (!outputFiles_[i]->is_open()) {
            std::cerr << "Failed to open output file: " << filename << std::endl;
            return false;
        }
    }

    // Create 256 demodulator instances
    instances_.reserve(numInstances_);

    for (size_t i = 0; i < numInstances_; ++i) {
        auto inst = std::make_unique<DemodInstance>();
        inst->handle = Demod_Create();
        if (!inst->handle) {
            std::cerr << "Failed to create demodulator instance " << i << std::endl;
            return false;
        }
        inst->threadId = i % threadPool_.getThreadCount();

        // Set channel parameter
        Demod_SetParam(inst->handle, "channel", static_cast<int>(i));

        // Register static callback for each instance
        // In real implementation with userdata, would use different approach
        Demod_SetDataCallback(inst->handle, demodStaticCallback);

        instances_.push_back(std::move(inst));
    }

    return true;
}

void DemodWrapper::processBlock(uint32_t channel, const uint8_t* data, size_t dataLen, int threadId) {
    (void)threadId;  // suppress unused warning

    if (channel >= numInstances_) {
        std::cerr << "Invalid channel: " << channel << std::endl;
        return;
    }

    // Convert uint8_t to Demod_16sc format
    size_t sampleCount = dataLen / 2;  // 2 bytes per I/Q sample

    // Allocate and fill samples
    Demod_16sc* samplesPtr = new Demod_16sc[sampleCount];
    for (size_t i = 0; i < sampleCount; ++i) {
        samplesPtr[i].i = static_cast<short>(data[i * 2]) - 128;  // Convert to signed
        samplesPtr[i].q = static_cast<short>(data[i * 2 + 1]) - 128;
    }

    // Copy handle for lambda
    DemodHandle handle = instances_[channel]->handle;

    // Submit to thread pool
    threadPool_.enqueue([handle, samplesPtr, sampleCount]() {
        Demod_Process(handle, samplesPtr);
        delete[] samplesPtr;
    });
}

void DemodWrapper::writeOutput(int threadId, const DemodOutput* output) {
    if (threadId >= 0 && threadId < static_cast<int>(outputFiles_.size()) && outputFiles_[threadId]) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        auto* file = outputFiles_[threadId];
        if (output && output->data && output->dataLen > 0) {
            file->write(reinterpret_cast<const char*>(output->data), output->dataLen);
            file->flush();
        }
    }
}