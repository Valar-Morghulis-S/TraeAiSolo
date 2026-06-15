#pragma once

#include "demod_api.h"
#include "thread_pool.h"
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <functional>

// Shared memory data header - 128 bytes with channel info
struct ShmDataHeader {
    uint32_t magic;        // Magic number for sync
    uint32_t channelId;   // Channel ID (0-255)
    uint32_t dataLen;      // Data length
    uint8_t  reserved[116]; // Reserved/padding
};

// Forward declarations
class Channel;
class Demodulator;
class Manager;

// ============================================================================
// Demodulator class - wraps demod_api instance
// ============================================================================
class Demodulator {
public:
    Demodulator(int channelId, ThreadPool& pool);
    ~Demodulator();

    // Process I/Q data through thread pool
    void processData(const uint8_t* data, size_t dataLen);

    // Get assigned thread ID
    int getThreadId() const { return threadId_; }

    // Set output callback for writing results
    void setOutputCallback(std::function<void(int threadId, const DemodOutput*)> callback);

private:
    int channelId_;
    DemodHandle handle_;
    ThreadPool& threadPool_;
    int threadId_;
    std::function<void(int, const DemodOutput*)> outputCallback_;

    void onDemodOutput(DemodOutput* output);
};

// ============================================================================
// Channel class - manages demodulator instance for a specific channel
// ============================================================================
class Channel {
public:
    Channel(int channelId, ThreadPool& pool);
    ~Channel();

    // Get channel ID
    int getChannelId() const { return channelId_; }

    // Process data for this channel
    void processData(const uint8_t* data, size_t dataLen);

    // Set output callback
    void setOutputCallback(std::function<void(int, const DemodOutput*)> callback);

private:
    int channelId_;
    std::unique_ptr<Demodulator> demodulator_;
};

// ============================================================================
// ReadThread class - reads from shared memory
// ============================================================================
class ReadThread {
public:
    ReadThread(Manager& manager);
    ~ReadThread();

    // Start reading from shared memory
    void start();

    // Stop reading
    void stop();

    // Check if running
    bool isRunning() const { return running_.load(); }

private:
    void readLoop();

    Manager& manager_;
    std::atomic<bool> running_;
    std::thread thread_;
};

// ============================================================================
// Manager class - manages all channels and dispatches to thread pool
// ============================================================================
class Manager {
public:
    Manager(size_t maxThreads = 8, size_t maxChannels = 256);
    ~Manager();

    // Initialize manager
    bool init();

    // Get thread pool
    ThreadPool& getThreadPool() { return threadPool_; }

    // Find or create channel by ID
    Channel* findOrCreateChannel(int channelId);

    // Set output callback for all channels
    void setOutputCallback(std::function<void(int threadId, const DemodOutput*)> callback);

    // Get channel count
    size_t getChannelCount() const { return channels_.size(); }

    // Wait for all tasks
    void waitAll();

    // Write output (called by demodulator callbacks)
    void writeOutput(int threadId, const DemodOutput* output);

private:
    size_t maxThreads_;
    size_t maxChannels_;
    ThreadPool threadPool_;
    std::map<int, std::unique_ptr<Channel>> channels_;
    std::mutex channelsMutex_;
    std::atomic<uint64_t> taskCounter_;
    std::function<void(int, const DemodOutput*)> outputCallback_;
    std::mutex outputMutex_;
    std::vector<std::ofstream*> outputFiles_;
};

// Helper function to convert uint8_t to Demod_16sc
inline void convertToDemodSamples(const uint8_t* data, size_t dataLen,
                                   std::vector<Demod_16sc>& samples) {
    size_t sampleCount = dataLen / 2;
    samples.resize(sampleCount);
    for (size_t i = 0; i < sampleCount; ++i) {
        samples[i].i = static_cast<short>(data[i * 2]) - 128;
        samples[i].q = static_cast<short>(data[i * 2 + 1]) - 128;
    }
}