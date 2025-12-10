#ifndef FAKE_IRIS_XE_GEM_HPP
#define FAKE_IRIS_XE_GEM_HPP

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSObject.h>


extern "C" void OSMemoryBarrier(void);
#define OSMemoryBarrier() __asm__ volatile("" ::: "memory")


class FakeIrisXEGEM : public OSObject {
    OSDeclareDefaultStructors(FakeIrisXEGEM);

public:
    static FakeIrisXEGEM* withSize(size_t size, uint32_t flags = 0);

    bool init() override;
    void free() override;

    bool allocate();
    void pin();
    void unpin();

    uint64_t physicalAddress() const { return fPhysAddr; }
    IOBufferMemoryDescriptor* memoryDescriptor() const { return fBuffer; }

    uint32_t pageCount() const { return (uint32_t)((fSize + 4095) / 4096); }

    mach_vm_address_t getPhysicalSegment(uint64_t offset, uint64_t* lengthOut);

private:
    IOBufferMemoryDescriptor* fBuffer;
    size_t fSize;
    mach_vm_address_t fPhysAddr;

    IOLock* fLock;
    uint32_t fPinCount;
    uint32_t fFlags;
    
private:
    uint64_t fGpuAddress = 0;

public:
    void setGpuAddress(uint64_t a) { fGpuAddress = a; }
    uint64_t gpuAddress() const { return fGpuAddress; }

    
    
    
    
};

#endif
