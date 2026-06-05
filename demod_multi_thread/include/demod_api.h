#pragma once

#include <cstdint>

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
    // other fields can be added here
};

// Demodulator API
#ifdef __cplusplus
extern "C" {
#endif

// Create demodulator instance
DemodHandle Demod_Create();

// Set parameter (variadic, specific params TBD)
int Demod_SetParam(DemodHandle handle, ...);

// Register data callback
int Demod_SetDataCallback(DemodHandle handle, void (*callback)(struct DemodOutput*));

// Input data for demodulation
int Demod_Process(DemodHandle handle, struct Demod_16sc* data);

// Destroy demodulator instance
void Demod_Destroy(DemodHandle handle);

#ifdef __cplusplus
}
#endif