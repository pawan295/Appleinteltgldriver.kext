#pragma once

#include <stdint.h>
#include <stddef.h>


class FakeIrisXEGEM;
class FakeIrisXEFramebuffer;

class FakeIrisXERing {
public:
    FakeIrisXERing(volatile uint32_t* mmioBase);
    ~FakeIrisXERing();

    // Allocate CPU-visible ring memory
    bool allocateRing(size_t bytes);

    // GPU address provided by GGTT mapping
    void attachRingGPUAddress(uint64_t gpuAddr);

    // Program MMIO registers (ring base, enable ring)
    void programRingBaseToHW();
    void enableRing();

    // Software ring operations
    void pushDword(uint32_t dword);
    void flushRingCpuCache();
    void updateHWTail();
    uint32_t readHWHead();

    FakeIrisXEFramebuffer* fOwner;

    
    
    // Submit a batch buffer GPU address using MI_BATCH_BUFFER_START
    bool submitBatch64(uint64_t batchGpuAddr);

    // Getter
    size_t size() const { return mRingSize; }
    uint64_t gpuAddr() const { return mRingGPUAddr; }

private:
    volatile uint32_t* mMMIO;   // BAR0 base
    uint32_t*          mRingCPU;
    size_t             mRingSize;
    uint64_t           mRingWriteOffset;
    uint64_t           mRingGPUAddr;
};
