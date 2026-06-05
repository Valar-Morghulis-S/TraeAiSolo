// Mock implementation of demod_api for testing
// This simulates the behavior of a real demodulation library

#include "demod_api.h"
#include <cstring>
#include <vector>
#include <mutex>
#include <cstdlib>
#include <ctime>
#include <atomic>

static std::mutex g_mock_mutex;
static std::atomic<int> g_mock_instance_count{0};

struct MockDemodData {
    int channel;
    void (*callback)(struct DemodOutput*);
    std::vector<uint8_t> accumBuffer;
    int instanceId;
};

static std::vector<MockDemodData*> g_mock_instances;
static MockDemodData* g_callback_instance_map[256] = {nullptr};

// Static callback function for C linkage
extern "C" {
static void mockDemodCallback(struct DemodOutput* output) {
    // Find which instance owns this callback and call its handler
    for (auto* inst : g_mock_instances) {
        if (inst && inst->callback) {
            inst->callback(output);
        }
    }
}
}

extern "C" {

DemodHandle Demod_Create() {
    std::lock_guard<std::mutex> lock(g_mock_mutex);
    auto* data = new MockDemodData();
    data->channel = g_mock_instance_count.fetch_add(1);
    data->callback = nullptr;
    data->instanceId = data->channel;

    if (data->instanceId < 256) {
        g_callback_instance_map[data->instanceId] = data;
    }
    g_mock_instances.push_back(data);
    return static_cast<DemodHandle>(data);
}

int Demod_SetParam(DemodHandle handle, ...) {
    if (!handle) return -1;
    // Simple mock: just accept params without doing much
    return 0;
}

int Demod_SetDataCallback(DemodHandle handle, void (*callback)(struct DemodOutput*)) {
    if (!handle) return -1;
    auto* data = static_cast<MockDemodData*>(handle);
    data->callback = callback;
    return 0;
}

int Demod_Process(DemodHandle handle, struct Demod_16sc* data) {
    if (!handle || !data) return -1;

    auto* mockData = static_cast<MockDemodData*>(handle);

    // Simulate demodulation by accumulating some data and triggering callback
    size_t sampleCount = 1; // Process 1 sample at a time for demo

    for (size_t i = 0; i < sampleCount; ++i) {
        uint8_t mag = static_cast<uint8_t>((data[i].i + 128 + data[i].q + 128) / 2);
        mockData->accumBuffer.push_back(mag);
    }

    // When buffer reaches certain size, trigger callback with processed data
    if (mockData->accumBuffer.size() >= 64 && mockData->callback) {
        DemodOutput output;
        output.data = mockData->accumBuffer.data();
        output.dataLen = static_cast<uint32_t>(mockData->accumBuffer.size());

        // Clear buffer after outputting
        mockData->accumBuffer.clear();

        mockData->callback(&output);
    }

    return 0;
}

void Demod_Destroy(DemodHandle handle) {
    if (!handle) return;
    auto* data = static_cast<MockDemodData*>(handle);

    std::lock_guard<std::mutex> lock(g_mock_mutex);
    for (auto it = g_mock_instances.begin(); it != g_mock_instances.end(); ++it) {
        if (*it == data) {
            g_mock_instances.erase(it);
            break;
        }
    }
    if (data->instanceId < 256) {
        g_callback_instance_map[data->instanceId] = nullptr;
    }
    delete data;
}

} // extern "C"