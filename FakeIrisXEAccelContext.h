#ifndef FakeIrisXEAccelContext_h
#define FakeIrisXEAccelContext_h

#pragma once
#include <IOKit/IOService.h>

class FakeIrisXEAccelerator;

#define super IOService
class FakeIrisXEAccelContext : public IOService {
    OSDeclareDefaultStructors(FakeIrisXEAccelContext);

public:
    virtual bool init() override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual void free() override;

    // metadata
    void setContextId(uint32_t id) { mCtxId = id; }
    uint32_t getContextId() const { return mCtxId; }

    void setOwner(FakeIrisXEAccelerator* a) { mOwner = a; }

    // Bind and present APIs used by Shared
    bool bindSurface_UserMapped(const void* userPtr, size_t bytes, uint32_t rowbytes, uint32_t w, uint32_t h);
    bool presentContext();

protected:
    uint32_t mCtxId;
    FakeIrisXEAccelerator* mOwner;

    // surface metadata
    uintptr_t surf_vaddr;
    size_t surf_bytes;
    uint32_t surf_rowbytes;
    uint32_t surf_w, surf_h;

    void* surfCPU;
    uint32_t surfWidth;
    uint32_t surfHeight;
    uint32_t surfRowBytes;




};







#endif /* FakeIrisXEAccelContext_h */
