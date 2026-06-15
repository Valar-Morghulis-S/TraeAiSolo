// Mock implementation of demod_api for testing
#include "demod_api.h"
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdlib>

static std::mutex g_mutex;
static std::atomic<int> g_instance_count{0};

struct MockDemodData {
    int channel;
    void (*callback)(struct DemodOutput*);
    std::vector<uint8_t> buffer;
};

static std::vector<MockDemodData*> g_instances;

extern "C" {

DemodHandle Demod_Create() {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto* data = new MockDemodData();
    data->channel = g_instance_count.fetch_add(1);
    data->callback = nullptr;
    g_instances.push_back(data);
    return static_cast<DemodHandle>(data);
}

int Demod_SetParam(DemodHandle handle, ...) {
    return handle ? 0 : -1;
}

int Demod_SetDataCallback(DemodHandle handle, void (*callback)(struct DemodOutput*)) {
    if (!handle) return -1;
    auto* data = static_cast<MockDemodData*>(handle);
    data->callback = callback;
    return 0;
}

int Demod_Process(DemodHandle handle, struct Demod_16sc* data) {
    if (!handle || !data) return -1;

    auto* mock = static_cast<MockDemodData*>(handle);

    // Accumulate samples
    uint8_t mag = static_cast<uint8_t>((data->i + 128 + data->q + 128) / 2);
    mock->buffer.push_back(mag);

    // Output when buffer is full
    if (mock->buffer.size() >= 64 && mock->callback) {
        DemodOutput output;
        output.data = mock->buffer.data();
        output.dataLen = static_cast<uint32_t>(mock->buffer.size());
        mock->buffer.clear();
        mock->callback(&output);
    }

    return 0;
}

void Demod_Destroy(DemodHandle handle) {
    if (!handle) return;
    auto* data = static_cast<MockDemodData*>(handle);

    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto it = g_instances.begin(); it != g_instances.end(); ++it) {
        if (*it == data) {
            g_instances.erase(it);
            break;
        }
    }
    delete data;
}

} // extern "C"