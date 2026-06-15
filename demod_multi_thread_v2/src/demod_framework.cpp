#include "demod_framework.h"
#include <iostream>
#include <cstring>
#include <fstream>

// ============================================================================
// Demodulator implementation
// ============================================================================
Demodulator::Demodulator(int channelId, ThreadPool& pool)
    : channelId_(channelId), threadPool_(pool), threadId_(channelId % pool.getThreadCount()) {
    handle_ = Demod_Create();
    if (handle_) {
        Demod_SetParam(handle_, "channel", channelId);
        // Note: Using static callback - in production, use userdata passing mechanism
        Demod_SetDataCallback(handle_, [](struct DemodOutput* output) {
            (void)output;
        });
    }
}

Demodulator::~Demodulator() {
    if (handle_) {
        Demod_Destroy(handle_);
        handle_ = nullptr;
    }
}

void Demodulator::processData(const uint8_t* data, size_t dataLen) {
    if (!handle_) return;

    // Convert uint8_t to Demod_16sc format
    size_t sampleCount = dataLen / 2;
    Demod_16sc* samples = new Demod_16sc[sampleCount];
    for (size_t i = 0; i < sampleCount; ++i) {
        samples[i].i = static_cast<short>(data[i * 2]) - 128;
        samples[i].q = static_cast<short>(data[i * 2 + 1]) - 128;
    }

    // Submit to thread pool - dispatch by channelId to specific thread
    DemodHandle handle = handle_;
    threadPool_.enqueue([handle, samples, sampleCount, this]() {
        Demod_Process(handle, samples);
        delete[] samples;
    });
}

void Demodulator::setOutputCallback(std::function<void(int, const DemodOutput*)> callback) {
    outputCallback_ = callback;
}

void Demodulator::onDemodOutput(DemodOutput* output) {
    if (outputCallback_) {
        outputCallback_(threadId_, output);
    }
}

// ============================================================================
// Channel implementation
// ============================================================================
Channel::Channel(int channelId, ThreadPool& pool)
    : channelId_(channelId) {
    demodulator_ = std::make_unique<Demodulator>(channelId, pool);
}

Channel::~Channel() = default;

void Channel::processData(const uint8_t* data, size_t dataLen) {
    if (demodulator_) {
        demodulator_->processData(data, dataLen);
    }
}

void Channel::setOutputCallback(std::function<void(int, const DemodOutput*)> callback) {
    if (demodulator_) {
        demodulator_->setOutputCallback(callback);
    }
}

// ============================================================================
// ReadThread implementation
// ============================================================================
ReadThread::ReadThread(Manager& manager)
    : manager_(manager), running_(false) {
}

ReadThread::~ReadThread() {
    stop();
}

void ReadThread::start() {
    if (running_.load()) return;
    running_ = true;
    thread_ = std::thread(&ReadThread::readLoop, this);
}

void ReadThread::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ReadThread::readLoop() {
    // This is a simulation - in real implementation, read from shared memory
    // The shared memory data format:
    //   - 128 byte header with channel ID at offset 4
    //   - Followed by data payload

    std::cout << "ReadThread started" << std::endl;

    // Simulate reading data blocks
    const size_t blockDataSize = 128;
    const size_t totalBlockSize = 128 + blockDataSize;  // header + data
    const uint32_t maxChannels = 256;

    // Simulate processing multiple blocks
    for (uint32_t block = 0; block < 1000 && running_.load(); ++block) {
        // Simulate shared memory data
        uint8_t buffer[totalBlockSize];
        memset(buffer, 0, totalBlockSize);

        // Fill simulated header
        ShmDataHeader* header = reinterpret_cast<ShmDataHeader*>(buffer);
        header->magic = 0xDDCCBBAA;
        header->channelId = block % maxChannels;  // Distribute across channels
        header->dataLen = blockDataSize;

        // Fill simulated data
        for (size_t i = 128; i < totalBlockSize; ++i) {
            buffer[i] = static_cast<uint8_t>((i * block) & 0xFF);
        }

        // Extract channel ID from header and dispatch
        uint32_t channelId = header->channelId;
        const uint8_t* data = buffer + 128;  // Skip header

        // Find channel and process
        Channel* channel = manager_.findOrCreateChannel(static_cast<int>(channelId));
        if (channel) {
            channel->processData(data, blockDataSize);
        }
    }

    std::cout << "ReadThread stopped" << std::endl;
}

// ============================================================================
// Manager implementation
// ============================================================================
Manager::Manager(size_t maxThreads, size_t maxChannels)
    : maxThreads_(maxThreads),
      maxChannels_(maxChannels),
      threadPool_(maxThreads),
      taskCounter_(0) {
}

Manager::~Manager() {
    for (auto* f : outputFiles_) {
        if (f) {
            f->close();
            delete f;
        }
    }
}

bool Manager::init() {
    // Create output files for each thread
    outputFiles_.resize(maxThreads_, nullptr);
    for (size_t i = 0; i < maxThreads_; ++i) {
        std::string filename = "thread_" + std::to_string(i) + "_output.bin";
        outputFiles_[i] = new std::ofstream(filename, std::ios::binary);
        if (!outputFiles_[i]->is_open()) {
            std::cerr << "Failed to open: " << filename << std::endl;
            return false;
        }
    }

    // Set output callback for all existing and future channels
    outputCallback_ = [this](int threadId, const DemodOutput* output) {
        this->writeOutput(threadId, output);
    };

    std::cout << "Manager initialized with " << maxThreads_ << " threads, "
              << maxChannels_ << " max channels" << std::endl;
    return true;
}

Channel* Manager::findOrCreateChannel(int channelId) {
    std::lock_guard<std::mutex> lock(channelsMutex_);

    auto it = channels_.find(channelId);
    if (it != channels_.end()) {
        return it->second.get();
    }

    // Create new channel
    if (channels_.size() >= maxChannels_) {
        std::cerr << "Max channels reached: " << maxChannels_ << std::endl;
        return nullptr;
    }

    auto channel = std::make_unique<Channel>(channelId, threadPool_);
    channel->setOutputCallback(outputCallback_);

    Channel* ptr = channel.get();
    channels_[channelId] = std::move(channel);

    std::cout << "Created channel " << channelId << " (total: " << channels_.size() << ")" << std::endl;

    return ptr;
}

void Manager::setOutputCallback(std::function<void(int, const DemodOutput*)> callback) {
    outputCallback_ = callback;
}

void Manager::waitAll() {
    threadPool_.waitAll();
}

void Manager::writeOutput(int threadId, const DemodOutput* output) {
    if (threadId >= 0 && threadId < static_cast<int>(outputFiles_.size())) {
        std::lock_guard<std::mutex> lock(outputMutex_);
        auto* file = outputFiles_[threadId];
        if (file && output && output->data && output->dataLen > 0) {
            file->write(reinterpret_cast<const char*>(output->data), output->dataLen);
        }
    }
}