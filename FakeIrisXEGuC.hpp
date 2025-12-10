//
//  FakeIrisXEGuC.hpp
//  FakeIrisXEFramebuffer
//
//  Created by Anomy on 03/12/25.
//

// FakeIrisXEGuC.hpp
#pragma once

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include "FakeIrisXEGEM.hpp"
#include "FakeIrisXEFramebuffer.hpp"


class FakeIrisXEGuC : public OSObject {
    OSDeclareDefaultStructors(FakeIrisXEGuC);
    
private:
    FakeIrisXEFramebuffer* fOwner;
    FakeIrisXEGEM* fGuCFwGem;
    FakeIrisXEGEM* fHuCFwGem;
    FakeIrisXEGEM* fDmcFwGem;
    
    // Firmware versions
    uint32_t fGuCVersion;
    uint32_t fHuCVersion;
    uint32_t fDmcVersion;
    
    // GuC log buffer
    FakeIrisXEGEM* fGuCLogGem;
    uint32_t fGuCLogSize;
    
public:
    static FakeIrisXEGuC* withOwner(FakeIrisXEFramebuffer* owner);
    
    // Initialization
    bool initGuC();
    bool loadGuCFirmware(const uint8_t* fwData, size_t fwSize);
    bool loadHuCFirmware(const uint8_t* fwData, size_t fwSize);
    bool loadDmcFirmware(const uint8_t* fwData, size_t fwSize);
    
    // Enable/Disable
    bool enableGuCSubmission();
    bool disableGuC();
    
    // Submission
    bool submitToGuC(FakeIrisXEGEM* batchGem, uint64_t* outFence);
    
    // Status
    bool isGuCReady();
    void dumpGuCStatus();
    
private:
   // bool setupGuCInterrupts();

    bool waitGuCReady(uint32_t timeoutMs = 5000);
    bool uploadFirmware(FakeIrisXEGEM* fwGem, uint32_t fwType);
};
