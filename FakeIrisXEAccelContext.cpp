// FakeIrisXEAccelContext.cpp
#include "FakeIrisXEAccelContext.h"
#include "FakeIrisXEAccelerator.hpp"
#include "FakeIrisXEFramebuffer.hpp"
#include <IOKit/IOLib.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOLib.h>
#include "FakeIrisXEAccelerator.hpp"
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOWorkLoop.h>



#include <IOKit/IOLib.h>

OSDefineMetaClassAndStructors(FakeIrisXEAccelContext, IOService);

bool FakeIrisXEAccelContext::init() {
    if (!super::init()) return false;
    mCtxId = 0;
    mOwner = nullptr;
    surf_vaddr = 0; surf_bytes = 0; surf_rowbytes = 0; surf_w = surf_h = 0;
    return true;
}

bool FakeIrisXEAccelContext::start(IOService* provider) {
    if (!super::start(provider)) return false;
    IOLog("(FakeIrisXEAccelContext) ctx %u started\n", mCtxId);
    return true;
}

void FakeIrisXEAccelContext::stop(IOService* provider) {
    IOLog("(FakeIrisXEAccelContext) ctx %u stop\n", mCtxId);
    super::stop(provider);
}

void FakeIrisXEAccelContext::free() {
    super::free();
}

bool FakeIrisXEAccelContext::bindSurface_UserMapped(
    const void* cpuPtr,
    size_t bytes,
    uint32_t rowBytes,
    uint32_t w,
    uint32_t h)
{
    if (!cpuPtr || w == 0 || h == 0)
        return false;

    surfCPU      = (void*)cpuPtr;
    surfRowBytes = rowBytes;
    surfWidth    = w;
    surfHeight   = h;

    IOLog("Context %u: bound surface %ux%u rowBytes=%u cpuPtr=%p\n",
         w, h, rowBytes, cpuPtr);

    return true;
}

