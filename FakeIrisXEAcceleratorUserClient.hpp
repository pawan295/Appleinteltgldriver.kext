#ifndef FAKE_IRIS_XE_ACCEL_USERCLIENT_HPP
#define FAKE_IRIS_XE_ACCEL_USERCLIENT_HPP

#include <IOKit/IOUserClient.h>

class FakeIrisXEAccelerator;
class FakeIrisXEGEM;
class GEMHandleTable;   // GLOBAL forward declaration

class FakeIrisXEAcceleratorUserClient : public IOUserClient {
    OSDeclareDefaultStructors(FakeIrisXEAcceleratorUserClient);

public:
    bool initWithTask(task_t owningTask, void* securityID, UInt32 type) override;
    bool start(IOService* provider) override;
    void stop(IOService* provider) override;

    IOReturn clientClose() override;
    IOReturn externalMethod(uint32_t selector,
                            IOExternalMethodArguments* args,
                            IOExternalMethodDispatch* dispatch,
                            OSObject* target, void* reference) override;

    IOReturn clientMemoryForType(UInt32 type, UInt32 *flags, IOMemoryDescriptor **memory) override;

    
    
    
private:
    FakeIrisXEAccelerator* fOwner;
    task_t fTask;

    // GEM
    GEMHandleTable* fHandleTable = nullptr;
    uint32_t fLastRequestedGemHandle = 0;

    uint32_t createGemAndRegister(uint64_t size, uint32_t flags);
    bool destroyGemHandle(uint32_t handle);
    IOReturn pinGemHandle(uint32_t handle, uint64_t* outGpuAddr);
    bool unpinGemHandle(uint32_t handle);
    IOReturn getPhysPagesForHandle(uint32_t handle, void* outBuf, size_t* outSize);
    
    
    

};

#endif
