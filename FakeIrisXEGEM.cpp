#include "FakeIrisXEGEM.hpp"

#define super OSObject
OSDefineMetaClassAndStructors(FakeIrisXEGEM, OSObject)

bool FakeIrisXEGEM::init() {
    if (!super::init()) return false;
    fBuffer = nullptr;
    fSize = 0;
    fPhysAddr = 0;
    fPinCount = 0;
    fFlags = 0;
    fLock = IOLockAlloc();
    return true;
}

void FakeIrisXEGEM::free() {
    if (fBuffer) {
        fBuffer->release();
        fBuffer = nullptr;
    }
    if (fLock) {
        IOLockFree(fLock);
        fLock = nullptr;
    }
    super::free();
}

FakeIrisXEGEM* FakeIrisXEGEM::withSize(size_t size, uint32_t flags) {
    FakeIrisXEGEM* obj = OSTypeAlloc(FakeIrisXEGEM);
    if (!obj) return nullptr;
    if (!obj->init()) { obj->release(); return nullptr; }

    obj->fSize = size;
    obj->fFlags = flags;

    if (!obj->allocate()) {
        obj->release();
        return nullptr;
    }

    return obj;
}

bool FakeIrisXEGEM::allocate() {
    fBuffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task,
        kIODirectionOutIn | kIOMemoryKernelUserShared,
        fSize,
        0xFFFFFFFFFFFFF000ULL
    );

    if (!fBuffer) return false;

    fPhysAddr = fBuffer->getPhysicalAddress();
    return (fPhysAddr != 0);
}

void FakeIrisXEGEM::pin() {
    IOLockLock(fLock);
    fPinCount++;
    IOLockUnlock(fLock);
}

void FakeIrisXEGEM::unpin() {
    IOLockLock(fLock);
    if (fPinCount > 0) fPinCount--;
    IOLockUnlock(fLock);
}

mach_vm_address_t FakeIrisXEGEM::getPhysicalSegment(uint64_t offset, uint64_t* lengthOut) {
    if (!fBuffer) return 0;
    return fBuffer->getPhysicalSegment(offset, lengthOut);
}
