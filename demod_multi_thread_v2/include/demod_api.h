#pragma once

#include <cstdint>
#include <functional>

// Demodulator handle type
typedef void* DemodHandle;

// Demod_16sc data structure
struct Demod_16sc {
    short i;
    short q;
};

// DemodOutput structure for callback
struct DemodOutput {
    uint8_t* data;
    uint32_t dataLen;
};

// Demodulator API
#ifdef __cplusplus
extern "C" {
#endif

DemodHandle Demod_Create();
int Demod_SetParam(DemodHandle handle, ...);
int Demod_SetDataCallback(DemodHandle handle, void (*callback)(struct DemodOutput*));
int Demod_Process(DemodHandle handle, struct Demod_16sc* data);
void Demod_Destroy(DemodHandle handle);

#ifdef __cplusplus
}
#endif