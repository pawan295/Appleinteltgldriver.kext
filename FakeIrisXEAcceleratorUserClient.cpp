#include "FakeIrisXEAcceleratorUserClient.hpp"
#include "FakeIrisXEAccelerator.hpp"
#include "FakeIrisXEGEM.hpp"
#include <IOKit/IOLib.h>
#include <libkern/c++/OSSymbol.h>
#include "FakeIrisXEFramebuffer.hpp"
#include "FakeIrisXEAccelShared.h"   // for kFakeIris_Method_SubmitExeclistFenceTest


#define super OSObject

class GEMHandleTable : public OSObject {
    OSDeclareDefaultStructors(GEMHandleTable);

public:
    OSDictionary* dict;
    IOLock* lock;
    uint32_t nextHandle;

    static GEMHandleTable* create() {
        GEMHandleTable* t = OSTypeAlloc(GEMHandleTable);
        if (!t || !t->init()) {
            if (t) t->release();
            return nullptr;
        }
        t->dict = OSDictionary::withCapacity(256);
        t->lock = IOLockAlloc();
        t->nextHandle = 1;
        return t;
    }

    bool init() override {
        if (!OSObject::init()) return false;
        dict = nullptr;
        lock = nullptr;
        nextHandle = 1;
        return true;
    }

    void free() override {
        if (dict) dict->release();
        if (lock) IOLockFree(lock);
        super::free();
    }

    uint32_t add(FakeIrisXEGEM* gem) {
        IOLockLock(lock);

        char keybuf[32];
        snprintf(keybuf, sizeof(keybuf), "%u", nextHandle);

        const OSSymbol* key = OSSymbol::withCString(keybuf);
        dict->setObject(key, gem);
        key->release();

        gem->retain();
        return nextHandle++;
    }

    FakeIrisXEGEM* lookup(uint32_t h) {
        IOLockLock(lock);

        char keybuf[32];
        snprintf(keybuf, sizeof(keybuf), "%u", h);

        const OSSymbol* key = OSSymbol::withCString(keybuf);
        OSObject* o = dict->getObject(key);
        key->release();

        FakeIrisXEGEM* gem = OSDynamicCast(FakeIrisXEGEM, o);
        if (gem) gem->retain();

        IOLockUnlock(lock);
        return gem;
    }

    bool remove(uint32_t h) {
        IOLockLock(lock);

        char keybuf[32];
        snprintf(keybuf, sizeof(keybuf), "%u", h);

        const OSSymbol* key = OSSymbol::withCString(keybuf);
        bool exists = dict->getObject(key) != nullptr;

        if (exists) {
            FakeIrisXEGEM* gem = OSDynamicCast(FakeIrisXEGEM, dict->getObject(key));
            if (gem) gem->release();
            dict->removeObject(key);
        }

        key->release();
        IOLockUnlock(lock);
        return exists;
    }
};

OSDefineMetaClassAndStructors(GEMHandleTable, OSObject)




#define super IOUserClient
OSDefineMetaClassAndStructors(FakeIrisXEAcceleratorUserClient, IOUserClient)

bool FakeIrisXEAcceleratorUserClient::initWithTask(task_t task, void* secID, UInt32 type)
{
    if (!super::initWithTask(task, secID, type)) return false;
    fTask = task;
    return true;
}

bool FakeIrisXEAcceleratorUserClient::start(IOService* provider)
{
    if (!super::start(provider)) return false;

    fOwner = OSDynamicCast(FakeIrisXEAccelerator, provider);
    if (!fOwner) return false;

    fHandleTable = GEMHandleTable::create();
    return true;
}

void FakeIrisXEAcceleratorUserClient::stop(IOService* provider)
{
    fOwner = nullptr;
    super::stop(provider);
}

IOReturn FakeIrisXEAcceleratorUserClient::clientClose() {
    return kIOReturnSuccess;
}


uint32_t FakeIrisXEAcceleratorUserClient::createGemAndRegister(uint64_t size, uint32_t flags)
{
    FakeIrisXEGEM* gem = FakeIrisXEGEM::withSize((size_t)size, flags);
    if (!gem) return 0;

    uint32_t handle = fHandleTable->add(gem);
    gem->release();
    return handle;
}

bool FakeIrisXEAcceleratorUserClient::destroyGemHandle(uint32_t h)
{
    return fHandleTable->remove(h);
}

IOReturn FakeIrisXEAcceleratorUserClient::pinGemHandle(uint32_t handle, uint64_t* outGpuAddr)
{
    FakeIrisXEGEM* gem = fHandleTable->lookup(handle);
    if (!gem) return kIOReturnNotFound;

    gem->pin();

    uint64_t gpuVA = fOwner->fFramebuffer->ggttMap(gem);
    gem->setGpuAddress(gpuVA);   // add this new method

    *outGpuAddr = gpuVA;

    gem->release();
    return kIOReturnSuccess;
}

bool FakeIrisXEAcceleratorUserClient::unpinGemHandle(uint32_t handle) {
    FakeIrisXEGEM* gem = fHandleTable->lookup(handle);
    if (!gem) return false;

    uint64_t gpuVA = gem->gpuAddress();    // retrieve
    uint32_t pages = gem->pageCount();

    fOwner->fFramebuffer->ggttUnmap(gpuVA, pages);
    gem->unpin();

    gem->release();
    return true;
}


IOReturn FakeIrisXEAcceleratorUserClient::getPhysPagesForHandle(
    uint32_t h, void* outBuf, size_t* outSize)
{
    FakeIrisXEGEM* gem = fHandleTable->lookup(h);
    if (!gem) return kIOReturnNotFound;

    uint32_t pages = gem->pageCount();
    size_t need = pages * sizeof(uint64_t);

    if (!outBuf || *outSize < need) {
        *outSize = need;
        gem->release();
        return kIOReturnNoSpace;
    }

    uint64_t offset = 0;
    uint64_t* arr = (uint64_t*)outBuf;

    for (uint32_t i = 0; i < pages; i++) {
        uint64_t len = 0;
        arr[i] = gem->getPhysicalSegment(offset, &len);
        offset += len ? len : 4096;
    }

    *outSize = need;
    gem->release();
    return kIOReturnSuccess;
}




IOReturn FakeIrisXEAcceleratorUserClient::clientMemoryForType(
    UInt32 type, UInt32* flags, IOMemoryDescriptor** mem)
{
    if (!mem) return kIOReturnBadArgument;

    uint32_t handle = fLastRequestedGemHandle;
    FakeIrisXEGEM* gem = fHandleTable->lookup(handle);
    if (!gem) return kIOReturnNotFound;

    IOBufferMemoryDescriptor* desc = gem->memoryDescriptor();
    if (!desc) {
        gem->release();
        return kIOReturnNoMemory;
    }

    desc->retain();
    *mem = desc;
    *flags = 0;

    gem->release();
    return kIOReturnSuccess;
}







IOReturn FakeIrisXEAcceleratorUserClient::externalMethod(
    uint32_t selector,
    IOExternalMethodArguments* args,
    IOExternalMethodDispatch* dispatch,
    OSObject* target,
    void* ref)
{
    IOLog("(FakeIrisXEFramebuffer) [UC] externalMethod selector=%u\n", selector);

    switch (selector) {

        case kFakeIris_Method_SubmitExeclistFenceTest:
        {
            IOLog("(FakeIrisXEFramebuffer) [UC] SubmitExeclistFenceTest called\n");

            if (!fOwner) {
                IOLog("(FakeIrisXEFramebuffer) [UC] no owner\n");
                return kIOReturnNotReady;
            }

            FakeIrisXEFramebuffer* fb = fOwner->getFramebufferOwner();
            if (!fb) {
                IOLog("(FakeIrisXEFramebuffer) [UC] no framebuffer owner\n");
                return kIOReturnNotReady;
            }

            
            FakeIrisXEExeclist* exec = fOwner->fExeclistFromFB;
            FakeIrisXERing* ring = fOwner->fRcsRingFromFB;

            if (!exec || !ring) {
                IOLog("❌ [UC] Missing exec=%p ring=%p\n", exec, ring);
                return kIOReturnNotReady;
            }



            
            FakeIrisXEGEM* batchGem = fb->createTinyBatchGem();
            if (!batchGem) {
                IOLog("(FakeIrisXEFramebuffer) [UC] createTinyBatchGem FAILED\n");
                return kIOReturnNoMemory;
            }

            bool ok = exec->submitBatchWithExeclist(fb, batchGem, 0, ring, 2000);


            batchGem->release();

            IOLog("(FakeIrisXEFramebuffer) [UC] SubmitExeclistFenceTest result=%d\n",
                  ok ? 1 : 0);
            return ok ? kIOReturnSuccess : kIOReturnError;
        }

        default:
            IOLog("(FakeIrisXEFramebuffer) [UC] externalMethod unsupported selector=%u\n",
                  selector);
            return kIOReturnUnsupported;
    }
}
