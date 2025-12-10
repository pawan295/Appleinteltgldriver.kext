// FakeIrisXESharedUserClient.hpp
#pragma once
#include <IOKit/IOUserClient.h>

class FakeIrisXEAccelDevice;

class FakeIrisXESharedUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(FakeIrisXESharedUserClient)

private:
    task_t                  fTask{nullptr};
    FakeIrisXEAccelDevice*  fOwner{nullptr};

public:
    bool initWithTask(task_t owningTask, void* securityID, UInt32 type) override;
    bool start(IOService* provider) override;
    void stop(IOService* provider) override;
    IOReturn clientClose(void) override;
    IOReturn clientDied(void) override;

    IOReturn externalMethod(uint32_t selector,
                            IOExternalMethodArguments* args,
                            IOExternalMethodDispatch*,
                            OSObject*,
                            void*) override;
    
    
    // Selectors
    enum {
        kAccelSel_Ping = 0,
        kAccelSel_GetCaps = 1
    };

    // Tiny capability blob you can extend later
    struct __attribute__((packed)) XEAccelCaps {
        uint32_t version;      // 1
        uint32_t metalSupported; // 0/1 (hint only)
    };
    
    
};
