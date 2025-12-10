// FakeIrisXESharedUserClient.cpp
#include "FakeIrisXESharedUserClient.hpp"
#include "FakeIrisXEAccelDevice.hpp"
#include "FakeIrisXEFramebuffer.hpp"

#define super IOUserClient
OSDefineMetaClassAndStructors(FakeIrisXESharedUserClient, IOUserClient)


bool FakeIrisXESharedUserClient::initWithTask(task_t owningTask, void*, UInt32)
{
    if (!super::initWithTask(owningTask, nullptr, 0)) return false;
    fTask = owningTask;
    return true;
}

bool FakeIrisXESharedUserClient::start(IOService* provider)
{
    if (!super::start(provider)) return false;
    fOwner = OSDynamicCast(FakeIrisXEAccelDevice, provider);
    if (!fOwner) {
        IOLog("(FakeIrisXESharedUserClient) provider is not FakeIrisXEAccelDevice\n");
        return false;
    }
    return true;
}

void FakeIrisXESharedUserClient::stop(IOService* provider)
{
    super::stop(provider);
}

IOReturn FakeIrisXESharedUserClient::clientClose(void)
{
    terminate();
    return kIOReturnSuccess;
}

IOReturn FakeIrisXESharedUserClient::clientDied(void)
{
    return clientClose();
}

IOReturn FakeIrisXESharedUserClient::externalMethod(uint32_t selector,
                                                    IOExternalMethodArguments* args,
                                                    IOExternalMethodDispatch*,
                                                    OSObject*,
                                                    void*)
{
    switch (selector) {
        case kAccelSel_Ping:
            return kIOReturnSuccess;

        case kAccelSel_GetCaps: {
            if (!args || !args->structureOutput) return kIOReturnBadArgument;
            if (args->structureOutputSize < sizeof(XEAccelCaps)) return kIOReturnMessageTooLarge;
            XEAccelCaps caps{};
            caps.version = 1;
            caps.metalSupported = 0; // keep 0 for now; we’ll flip when ready
            bcopy(&caps, args->structureOutput, sizeof(caps));
            args->structureOutputSize = sizeof(caps);
            return kIOReturnSuccess;
        }

        default:
            return kIOReturnUnsupported;
    }
}
