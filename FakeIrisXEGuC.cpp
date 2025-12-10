//
//  FakeIrisXEGuC.cpp
//  FakeIrisXEFramebuffer
//
//  Created by Anomy on 03/12/25.
//

// FakeIrisXEGuC.cpp
#include "FakeIrisXEGuC.hpp"

#define super OSObject

OSDefineMetaClassAndStructors(FakeIrisXEGuC, OSObject);

// TGL GuC registers
#define GEN11_GUC_SOFT_SCRATCH(n)       (0x1C180 + (n) * 4)
#define GEN11_GUC_CTL                    0x1C0B0
#define GEN11_GUC_STATUS                 0x1C0B4
#define GEN11_GUC_CAPS1                  0x1C0A0
#define GEN11_GUC_CAPS2                  0x1C0A4
#define GEN11_GUC_CAPS3                  0x1C0A8
#define GEN11_GUC_CAPS4                  0x1C0AC
#define GEN11_GUC_FW_SIZE                0x1C0B8
#define GEN11_GUC_FW_ADDR_LO             0x1C0C4
#define GEN11_GUC_FW_ADDR_HI             0x1C0C8
#define GEN11_GUC_RESET                  0x1C0C0
#define GEN11_GUC_LOG_ADDR_LO            0x1C0D0
#define GEN11_GUC_LOG_ADDR_HI            0x1C0D4
#define GEN11_GUC_LOG_SIZE               0x1C0D8
#define GEN11_GUC_IRQ_CLEAR              0x1C0C8
#define GEN11_GUC_IRQ_ENABLE             0x1C0CC

// HuC registers
#define GEN11_HUC_FW_ADDR_LO             0x1C0E0
#define GEN11_HUC_FW_ADDR_HI             0x1C0E4
#define GEN11_HUC_STATUS                 0x1C0E8

FakeIrisXEGuC* FakeIrisXEGuC::withOwner(FakeIrisXEFramebuffer* owner)
{
    FakeIrisXEGuC* obj = OSTypeAlloc(FakeIrisXEGuC);
    if (!obj) return nullptr;
    
    if (!obj->init()) {
        obj->release();
        return nullptr;
    }
    
    obj->fOwner = owner;
    return obj;
}

bool FakeIrisXEGuC::initGuC()
{
    IOLog("(FakeIrisXE) [GuC] Initializing Tiger Lake GuC\n");
    
    // 1. Dump GuC capabilities
    uint32_t caps1 = fOwner->safeMMIORead(GEN11_GUC_CAPS1);
    uint32_t caps2 = fOwner->safeMMIORead(GEN11_GUC_CAPS2);
    uint32_t caps3 = fOwner->safeMMIORead(GEN11_GUC_CAPS3);
    uint32_t caps4 = fOwner->safeMMIORead(GEN11_GUC_CAPS4);
    
    IOLog("(FakeIrisXE) [GuC] Capabilities:\n");
    IOLog("  CAPS1: 0x%08x\n", caps1);
    IOLog("  CAPS2: 0x%08x\n", caps2);
    IOLog("  CAPS3: 0x%08x\n", caps3);
    IOLog("  CAPS4: 0x%08x\n", caps4);
    
    // Extract version support
    uint32_t supportedMajor = (caps1 >> 16) & 0xFF;
    uint32_t supportedMinor = (caps1 >> 8) & 0xFF;
    IOLog("(FakeIrisXE) [GuC] Supported version: %u.%u\n",
          supportedMajor, supportedMinor);
    
    // 2. Reset GuC
    IOLog("(FakeIrisXE) [GuC] Resetting GuC...\n");
    fOwner->safeMMIOWrite(GEN11_GUC_RESET, 0x1);
    (void)fOwner->safeMMIORead(GEN11_GUC_RESET);
    IOSleep(10);
    
    // Wait for reset complete
    uint64_t start = mach_absolute_time();
    uint64_t timeout = 100 * 1000000ULL; // 100ms
    
    while (mach_absolute_time() - start < timeout) {
        uint32_t status = fOwner->safeMMIORead(GEN11_GUC_STATUS);
        if (!(status & 0x8000)) { // Reset bit cleared
            break;
        }
        IOSleep(1);
    }
    
    // 3. Clear interrupts
    fOwner->safeMMIOWrite(GEN11_GUC_IRQ_CLEAR, 0xFFFFFFFF);
    fOwner->safeMMIOWrite(GEN11_GUC_IRQ_ENABLE, 0x0);
    
    // 4. Disable GuC initially
    fOwner->safeMMIOWrite(GEN11_GUC_CTL, 0x0);
    
    IOLog("(FakeIrisXE) [GuC] Initialization complete\n");
    return true;
}





bool FakeIrisXEGuC::loadGuCFirmware(const uint8_t* fwData, size_t fwSize)
{
    if (!fwData || fwSize < 4096) {
        IOLog("(FakeIrisXE) [GuC] Invalid firmware data\n");
        return false;
    }
    
    // Check for new CSS header format (type 0x6)
    uint32_t headerType = *(uint32_t*)fwData;
    size_t payloadOffset, payloadSize;
    
    if (headerType == 0xABCD) {
        // Old format
        struct GuCFirmwareHeader {
            uint32_t headerMarker;    // 0xABCD
            uint32_t headerLen;
            uint32_t headerVersion;
            uint32_t uCodeVersion;
            uint32_t uCodeLen;
            uint32_t uCodeCRC;
            uint32_t reserved[2];
        } __attribute__((packed));
        
        const GuCFirmwareHeader* header = (const GuCFirmwareHeader*)fwData;
        fGuCVersion = header->uCodeVersion;
        payloadSize = header->uCodeLen;
        payloadOffset = sizeof(GuCFirmwareHeader);
        
        IOLog("(FakeIrisXE) [GuC] Old format firmware v%u\n", fGuCVersion);
    }
    else if (headerType == 0x00000006) {
        // New CSS header format (from your logs)
        struct CSSFirmwareHeader {
            uint32_t module_type;     // 0x00000006
            uint32_t header_len;      // 0xA1 (161 bytes)
            uint32_t header_version;  // 0x10000
            uint32_t module_id;
            uint32_t module_vendor;   // 0x8086 (Intel)
            uint32_t date;
            uint32_t size;            // Total module size
            uint32_t key_size;
            uint32_t modulus_size;
            uint32_t exponent_size;
            uint32_t reserved[22];
            // Followed by modulus, exponent, signature
        } __attribute__((packed));
        
        const CSSFirmwareHeader* cssHeader = (const CSSFirmwareHeader*)fwData;
        fGuCVersion = cssHeader->header_version;  // 0x10000
        
        // Payload starts after header_len
        payloadOffset = cssHeader->header_len;
        // Payload size = total size - header_len
        payloadSize = fwSize - payloadOffset;
        
        IOLog("(FakeIrisXE) [GuC] New CSS format firmware v%u\n", fGuCVersion);
        IOLog("(FakeIrisXE) [GuC] Header len: 0x%x bytes\n", cssHeader->header_len);
        IOLog("(FakeIrisXE) [GuC] Payload: offset=0x%zx, size=0x%zx\n",
              payloadOffset, payloadSize);
    }
    else {
        IOLog("(FakeIrisXE) [GuC] Unknown firmware header: 0x%08x\n", headerType);
        return false;
    }
    
    // Validate payload
    if (payloadSize == 0 || payloadOffset + payloadSize > fwSize) {
        IOLog("(FakeIrisXE) [GuC] Invalid payload size\n");
        return false;
    }
    
    // Allocate GEM (aligned to 4K)
    size_t allocSize = (payloadSize + 4095) & ~4095;
    fGuCFwGem = FakeIrisXEGEM::withSize(allocSize, 0);
    if (!fGuCFwGem) {
        IOLog("(FakeIrisXE) [GuC] Failed to allocate GEM for firmware\n");
        return false;
    }
    
    // Copy firmware payload
    IOBufferMemoryDescriptor* md = fGuCFwGem->memoryDescriptor();
    void* cpuPtr = md->getBytesNoCopy();
    memcpy(cpuPtr, fwData + payloadOffset, payloadSize);
    
    // Pin and map
    fGuCFwGem->pin();
    uint64_t gpuAddr = fOwner->ggttMap(fGuCFwGem);
    if (!gpuAddr) {
        IOLog("(FakeIrisXE) [GuC] Failed to map firmware\n");
        fGuCFwGem->unpin();
        fGuCFwGem->release();
        fGuCFwGem = nullptr;
        return false;
    }
    
    IOLog("(FakeIrisXE) [GuC] Firmware mapped at GGTT=0x%llx\n", gpuAddr);
    
    // Program firmware address
    fOwner->safeMMIOWrite(GEN11_GUC_FW_ADDR_LO, (uint32_t)(gpuAddr & 0xFFFFFFFF));
    fOwner->safeMMIOWrite(GEN11_GUC_FW_ADDR_HI, (uint32_t)(gpuAddr >> 32));
    fOwner->safeMMIOWrite(GEN11_GUC_FW_SIZE, (uint32_t)(allocSize / 4096));
    
    return true;
}







bool FakeIrisXEGuC::loadHuCFirmware(const uint8_t* fwData, size_t fwSize)
{
    // Similar to GuC loading but for HuC
    if (!fwData || fwSize < 4096) {
        IOLog("(FakeIrisXE) [HuC] Invalid firmware data\n");
        return false;
    }
    
    // Parse HuC firmware header (similar to GuC)
    struct HuCFirmwareHeader {
        uint32_t headerMarker;    // 0xABCD or 0xFEED
        uint32_t headerLen;
        uint32_t uCodeVersion;
        uint32_t uCodeLen;
    } __attribute__((packed));
    
    const HuCFirmwareHeader* header = (const HuCFirmwareHeader*)fwData;
    fHuCVersion = header->uCodeVersion;
    size_t payloadSize = header->uCodeLen;
    
    IOLog("(FakeIrisXE) [HuC] Loading firmware v%u, size: 0x%zx\n",
          fHuCVersion, payloadSize);
    
    // Allocate and load HuC firmware
    size_t allocSize = (payloadSize + 4095) & ~4095;
    fHuCFwGem = FakeIrisXEGEM::withSize(allocSize, 0);
    if (!fHuCFwGem) return false;
    
    IOBufferMemoryDescriptor* md = fHuCFwGem->memoryDescriptor();
    void* cpuPtr = md->getBytesNoCopy();
    memcpy(cpuPtr, fwData + sizeof(HuCFirmwareHeader), payloadSize);
    
    fHuCFwGem->pin();
    uint64_t gpuAddr = fOwner->ggttMap(fHuCFwGem);
    
    fOwner->safeMMIOWrite(GEN11_HUC_FW_ADDR_LO, (uint32_t)(gpuAddr & 0xFFFFFFFF));
    fOwner->safeMMIOWrite(GEN11_HUC_FW_ADDR_HI, (uint32_t)(gpuAddr >> 32));
    
    return true;
}

bool FakeIrisXEGuC::enableGuCSubmission()
{
    IOLog("(FakeIrisXE) [GuC] Enabling GuC submission mode\n");
    
    if (!fGuCFwGem) {
        IOLog("(FakeIrisXE) [GuC] No firmware loaded\n");
        return false;
    }
    
   
    // 2. Enable GuC with submission
    uint32_t guc_ctl = 0;
    guc_ctl |= (1 << 0);   // Enable GuC
    guc_ctl |= (1 << 6);   // Enable submission
    guc_ctl |= (1 << 7);   // Load GuC
    
    if (fHuCFwGem) {
        guc_ctl |= (1 << 8);   // Load HuC
    }
    
    fOwner->safeMMIOWrite(GEN11_GUC_CTL, guc_ctl);
    
    // 3. Wait for GuC ready
    if (!waitGuCReady(10000)) { // 10 second timeout
        IOLog("(FakeIrisXE) [GuC] Failed to start GuC\n");
        return false;
    }
    
    // 4. Setup interrupts
   // setupGuCInterrupts();
    
    // 5. Check HuC status if loaded
    if (fHuCFwGem) {
        uint32_t huc_status = fOwner->safeMMIORead(GEN11_HUC_STATUS);
        IOLog("(FakeIrisXE) [HuC] Status: 0x%08x\n", huc_status);
    }
    
    IOLog("(FakeIrisXE) [GuC] GuC submission enabled successfully\n");
  dumpGuCStatus();
    
    return true;
}

bool FakeIrisXEGuC::waitGuCReady(uint32_t timeoutMs)
{
    uint64_t start = mach_absolute_time();
    uint64_t timeout = timeoutMs * 1000000ULL;
    
    IOLog("(FakeIrisXE) [GuC] Waiting for GuC ready (timeout: %u ms)...\n", timeoutMs);
    
    while (mach_absolute_time() - start < timeout) {
        uint32_t status = fOwner->safeMMIORead(GEN11_GUC_STATUS);
        
        // Check ready bits:
        // Bit 0: GuC ready
        // Bit 1: Firmware loaded
        // Bit 2: GuC communication established
        if ((status & 0x7) == 0x7) {
            IOLog("(FakeIrisXE) [GuC] Ready! Status: 0x%08x\n", status);
            return true;
        }
        
        // Check for errors
        if (status & 0xFFFF0000) {
            IOLog("(FakeIrisXE) [GuC] Error detected: 0x%08x\n", status);
            return false;
        }
        
        if ((mach_absolute_time() - start) % 1000000000ULL == 0) {
            IOLog("(FakeIrisXE) [GuC] Still waiting... Status: 0x%08x\n", status);
        }
        
        IOSleep(10);
    }
    
    IOLog("(FakeIrisXE) [GuC] Timeout waiting for GuC ready\n");
    return false;
}

void FakeIrisXEGuC::dumpGuCStatus()
{
    uint32_t status = fOwner->safeMMIORead(GEN11_GUC_STATUS);
    uint32_t ctl = fOwner->safeMMIORead(GEN11_GUC_CTL);
    
    IOLog("(FakeIrisXE) [GuC] Status Dump:\n");
    IOLog("  CTL: 0x%08x\n", ctl);
    IOLog("  STATUS: 0x%08x\n", status);
    IOLog("    Ready: %s\n", (status & 0x1) ? "YES" : "NO");
    IOLog("    FW Loaded: %s\n", (status & 0x2) ? "YES" : "NO");
    IOLog("    Comm Established: %s\n", (status & 0x4) ? "YES" : "NO");
    
    // Dump scratch registers
    for (int i = 0; i < 16; i++) {
        uint32_t val = fOwner->safeMMIORead(GEN11_GUC_SOFT_SCRATCH(i));
        IOLog("  Scratch[%02d]: 0x%08x\n", i, val);
    }
}
