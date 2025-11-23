#include "FakeIrisXEFramebuffer.hpp"
#include <IOKit/IOLib.h>
#include <libkern/libkern.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <libkern/c++/OSSymbol.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/graphics/IOAccelerator.h>
#include <IOKit/IOKitKeys.h>           // Needed for types like OSAsyncReference
#include <IOKit/IOUserClient.h>        // Must follow after including IOKit headers
#include <IOKit/IOMessage.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <string.h>
#include <IOKit/graphics/IOFramebufferShared.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOInterruptEventSource.h>
#include <libkern/OSAtomic.h>
#include <pexpert/i386/boot.h>            // PE_Video

extern "C" {
    #include <pexpert/pexpert.h>
    #include <pexpert/device_tree.h>
    #include <libkern/OSTypes.h>
}


using namespace libkern;



// Connection attribute keys (from IOFramebufferShared.h, internal Apple headers)
#define kConnectionSupportsAppleSense   0x00000001
#define kConnectionSupportsLLDDCSense   0x00000002
#define kConnectionSupportsHLDDCSense   0x00000004
#define kConnectionSupportsDDCSense     0x00000008
#define kConnectionDisplayParameterCount 0x00000009
#define kConnectionFlags                0x0000000A
#define kConnectionSupportsHotPlug        0x00000001
#define kIOFBCursorSupportedKey               "IOFBCursorSupported"
#define kIOFBHardwareCursorSupportedKey       "IOFBHardwareCursorSupported"
#define kIOFBDisplayModeCountKey              "IOFBDisplayModeCount"
#define kIOFBNotifyDisplayModeChange 'dmod'
#define kIOTimingIDDefault 0

#define kIOFramebufferConsoleKey "IOFramebufferIsConsole"
#define kIOFBConsoleKey "kIOFramebufferConsoleKey"
#define kIO32BGRAPixelFormat 'BGRA'
#define kIO32ARGBPixelFormat 'ARGB'

#define kIOPixelFormatWideGamut 'wgam'
#define kIOCaptureAttribute 'capt'

#define kIOFBNotifyDisplayAdded  0x00000010
#define kIOFBConfigChanged       0x00000020

// IOFramebuffer-related property keys (manually declared)
#define kIOFBSurfaceKey                  "IOFBSurface"
#define kIOFBUserClientClassKey         "IOFBUserClientClass"
#define kIOFBSharedUserClientKey        "IOFBSharedUserClient"
#define kIOConsoleFramebuffer         "IOConsoleFramebuffer"
#define kIOConsoleSafeBoot            "IOConsoleSafe"
#define kIOConsoleDeviceKey           "IOConsoleDevice"
#define kIOKitConsoleSecurityKey      "IOKitConsoleSecurity"
#define kIOFBFramebufferKey     "IOFBFramebufferKey"
#define kIOConsoleFramebufferKey "IOConsoleFramebuffer"
#define kIOFramebufferIsConsoleKey "IOFramebufferIsConsole"
#define kIOConsoleModeKey "IOConsoleMode"
#define kIOFBNotifyConsoleReady  0x00002222
#define kIOFBNotifyDisplayModeChanged 0x00002223


#ifndef kIOTimingInfoValid_AppleTimingID
#define kIOTimingInfoValid_AppleTimingID 0x00000001
#endif

#ifndef kIOFBVsyncNotification
#define kIOFBVsyncNotification iokit_common_msg(0x300)
#endif

#define MAKE_IOVRAM_RANGE_INDEX(index) ((UInt32)(index))
#define kIOFBMemoryCountKey   "IOFBMemoryCount"

#define kIOFBVRAMMemory 0
#define kIOFBCursorMemory 1

// Connection flag values
#define kIOConnectionBuiltIn            0x00000100
#define kIOConnectionDisplayPort        0x00000800

#define kIOMessageServiceIsRunning 0x00001001

#ifndef kConnectionIsOnline
#define kConnectionIsOnline        'ionl'
#endif


#define SAFE_MMIO_WRITE(offset, value) \
    if (offset > mmioMap->getLength() - 4) { \
        IOLog("❌ MMIO offset 0x%X out of bounds\n", offset); \
        return kIOReturnError; \
    } \
    *(volatile uint32_t*)((uint8_t*)mmioBase + offset) = value;



#define super IOFramebuffer

OSDefineMetaClassAndStructors(FakeIrisXEFramebuffer, IOFramebuffer);



//probe
IOService *FakeIrisXEFramebuffer::probe(IOService *provider, SInt32 *score) {
    IOPCIDevice *pdev = OSDynamicCast(IOPCIDevice, provider);
    if (!pdev) {
        IOLog("FakeIrisXEFramebuffer::probe(): Provider is not IOPCIDevice\n");
        return nullptr;
    }

    UInt16 vendor = pdev->configRead16(kIOPCIConfigVendorID);
    UInt16 device = pdev->configRead16(kIOPCIConfigDeviceID);

    // Only proceed if it's your target device
    if (vendor == 0x8086 && device == 0x9A49) {
        IOLog("FakeIrisXEFramebuffer::probe(): Found matching GPU (8086:9A49)\n");
        
        
        if (score) *score = 99999999; // MAX override score
                return this; // 👈 Do NOT call super::probe() or it might lower the score!
            }

    return nullptr; // No match
}




bool FakeIrisXEFramebuffer::init(OSDictionary* dict) {
    if (!super::init(dict))
        return false;
   
// Initialize other members
    vramMemory = nullptr;
  //  mmioBase = nullptr;
   // mmioWrite32 = nullptr;
    currentMode = 0;
    currentDepth = 0;
    vramSize = 1920 * 1080 * 4;
    controllerEnabled = false;
    displayOnline = false;
    displayPublished = false;
    shuttingDown = false;
    fullyInitialized = false;  // ADD THIS
      
    
    
    
    return true;
}

IOPMPowerState FakeIrisXEFramebuffer::powerStates[kNumPowerStates] = {
    {
        1,                          // version
        0,                          // capabilityFlags
        0,                          // outputPowerCharacter
        0,                          // inputPowerRequirement
        0,                          // staticPower
        0,                          // unbudgetedPower
        0,                          // powerToAttain
        0,                          // timeToAttain
        0,                          // settleUpTime
        0,                          // timeToLower
        0,                          // settleDownTime
        0                           // powerDomainBudget
    },
    
    {
        1,                          // version
        IOPMPowerOn,                // capabilityFlags
        IOPMPowerOn,                // outputPowerCharacter
        IOPMPowerOn,                // inputPowerRequirement
        0, 0, 0, 0, 0, 0, 0
    }
};


    IOPCIDevice* pciDevice;
    IOMemoryMap* mmioMap;
    volatile uint8_t* mmioBase;


// --- CRITICAL MMIO HELPER FUNCTIONS ---
  // These functions ensure safe access to the memory-mapped registers.
  // They are essential for the power management block to compile and run.
inline uint32_t safeMMIORead(uint32_t offset){
      if (!mmioBase || !mmioMap || offset >= mmioMap->getLength()) {
          IOLog("❌ MMIO Read attempted with invalid offset: 0x%08X\n", offset);
          return 0;
      }
      return *(volatile uint32_t*)(mmioBase + offset);
  }

  inline void safeMMIOWrite(uint32_t offset, uint32_t value) {
      if (!mmioBase || !mmioMap || offset >= mmioMap->getLength()) {
          IOLog("❌ MMIO Write attempted with invalid offset: 0x%08X\n", offset);
          return;
      }
      *(volatile uint32_t*)(mmioBase + offset) = value;
  }



/* FakeIrisXEFramebuffer.cpp */

bool FakeIrisXEFramebuffer::initPowerManagement() {
    IOLog("🚀 Initiating CORRECTED-V24 power management (Enabling Clocks)...\n");

    if (!pciDevice || !pciDevice->isOpen(this)) {
        IOLog("❌ initPowerManagement(): PCI device not open - aborting\n");
        return false;
    }
    
    // --- PCI Power Management (Force D0) ---
    uint16_t pmcsr = pciDevice->configRead16(0x84);
    IOLog("PCI PMCSR before = 0x%04X\n", pmcsr);
    pmcsr &= ~0x3; // Force D0
    pciDevice->configWrite16(0x84, pmcsr);
    IOSleep(10);
    pmcsr = pciDevice->configRead16(0x84);
    IOLog("PCI PMCSR after force = 0x%04X\n", pmcsr);

    // --- Hardware Register Defines ---
    const uint32_t GT_PG_ENABLE = 0xA218;
    const uint32_t PUNIT_PG_CTRL = 0xA2B0;
    
    // PW1 (Render)
    const uint32_t PWR_WELL_CTL_1 = 0x45400;
    const uint32_t PWR_WELL_STATUS = 0x45408;
    const uint32_t PW_1_STATUS_BIT = (1 << 30);

    // PW2 (Display)
    const uint32_t PWR_WELL_CTL_2 = 0x45404;
    const uint32_t PW_2_REQ_BIT = (1 << 0);
    const uint32_t PW_2_STATE_VALUE = 0x000000FF;

    // Force wake
    const uint32_t FORCEWAKE_RENDER_CTL = 0xA188;
    const uint32_t FORCEWAKE_ACK_RENDER = 0x130044;
    const uint32_t RENDER_WAKE_VALUE = 0x000F000F; // Aggressive
    const uint32_t RENDER_ACK_BIT = 0x00000001;

    // MBUS
    const uint32_t MBUS_DBOX_CTL_A = 0x7003C;
    const uint32_t MBUS_DBOX_VALUE = 0xb1038c02;

    // --- V24 NEW CLOCK REGISTERS ---
    const uint32_t LCPLL1_CTL = 0x46010;
    const uint32_t LCPLL1_VALUE = 0xcc000000;
    const uint32_t TRANS_CLK_SEL_A = 0x46140;
    const uint32_t TRANS_CLK_VALUE = 0x10000000;

    // 1. GT Power Gating Control
    safeMMIOWrite(GT_PG_ENABLE, safeMMIORead(GT_PG_ENABLE) & ~0x1);
    IOSleep(10);

    // 2. PUNIT Power Gating Control
    safeMMIOWrite(PUNIT_PG_CTRL, safeMMIORead(PUNIT_PG_CTRL) & ~0x80000000);
    IOSleep(15);

    // 3. Power Well 1 Control (KNOWN GOOD)
    IOLog("Requesting Power Well 1 (Render)...\n");
    safeMMIOWrite(PWR_WELL_CTL_1, safeMMIORead(PWR_WELL_CTL_1) | 0x2);
    IOSleep(10);
    safeMMIOWrite(PWR_WELL_CTL_1, safeMMIORead(PWR_WELL_CTL_1) | 0x4);
    IOSleep(10);

    // 4. VERIFY Power Well 1 (KNOWN GOOD)
    IOLog("Waiting for Power Well 1 to be enabled...\n");
    int tries = 0;
    bool pw1_up = false;
    while (tries++ < 20) {
        if (safeMMIORead(PWR_WELL_STATUS) & PW_1_STATUS_BIT) {
            pw1_up = true;
            IOLog("✅ Power Well 1 is UP! Status: 0x%08X\n", safeMMIORead(PWR_WELL_STATUS));
            break;
        }
        IOSleep(10);
    }
    if (!pw1_up) {
        IOLog("❌ ERROR: Power Well 1 FAILED to enable! Status: 0x%08X\n", safeMMIORead(PWR_WELL_STATUS));
        return false;
    }

    // 5. Power Well 2 Control (KNOWN GOOD)
    IOLog("Requesting Power Well 2 (Display) via bit 0...\n");
    safeMMIOWrite(PWR_WELL_CTL_2, safeMMIORead(PWR_WELL_CTL_2) | PW_2_REQ_BIT);

    // 6. VERIFY Power Well 2 (KNOWN GOOD)
    IOLog("Waiting for Power Well 2 to be enabled (polling 0x45404 for 0xFF)...\n");
    tries = 0;
    bool pw2_up = false;
    while (tries++ < 50) {
        uint32_t pw2_status = safeMMIORead(PWR_WELL_CTL_2);
        if ((pw2_status & 0xFF) == PW_2_STATE_VALUE) {
            pw2_up = true;
            IOLog("✅ Power Well 2 is UP! Status: 0x%08X\n", pw2_status);
            break;
        }
        IOSleep(10);
    }
    if (!pw2_up) {
        IOLog("❌ ERROR: Power Well 2 FAILED to enable! Status: 0x%08X\n", safeMMIORead(PWR_WELL_CTL_2));
        return false;
    }

    // 7. FORCEWAKE Sequence (KNOWN GOOD)
    IOLog("Initiating AGGRESSIVE FORCEWAKE (0xF)...\n");
    safeMMIOWrite(FORCEWAKE_RENDER_CTL, RENDER_WAKE_VALUE); // Write 0x000F000F
    
    bool forcewake_ack = false;
    for (int i = 0; i < 100; i++) {
        uint32_t ack = safeMMIORead(FORCEWAKE_ACK_RENDER);
        if ((ack & RENDER_ACK_BIT) == RENDER_ACK_BIT) {
            forcewake_ack = true;
            IOLog("✅ Render ACK received! (0x%08X)\n", ack);
            break;
        }
        IOSleep(10);
    }
    if (!forcewake_ack) {
        IOLog("❌ ERROR: Render force-wake FAILED!\n");
        return false;
    }

    // 8. ENABLE DISPLAY MMIO BUS (KNOWN GOOD)
    IOLog("Enabling Display MMIO Bus (MBUS_DBOX_CTL_A)...\n");
    safeMMIOWrite(MBUS_DBOX_CTL_A, MBUS_DBOX_VALUE);
    IOSleep(10);

    // 9. --- NEW STEP: ENABLE DISPLAY CLOCKS ---
    IOLog("Enabling Display PLL (LCPLL1_CTL)...\n");
    safeMMIOWrite(LCPLL1_CTL, LCPLL1_VALUE);
    IOSleep(10);
    
    IOLog("Enabling Transcoder Clock Select (TRANS_CLK_SEL_A)...\n");
    safeMMIOWrite(TRANS_CLK_SEL_A, TRANS_CLK_VALUE);
    IOSleep(10);
    
    IOLog("Power management sequence complete.\n");
    return true;
}






//start
bool FakeIrisXEFramebuffer::start(IOService* provider) {
    IOLog("FakeIrisXEFramebuffer::start() - Entered\n");

    if (!super::start(provider)) {
        IOLog("FakeIrisXEFramebuffer::start() - super::start() failed\n");
        return false;
    }

    

    /*
    // Improved ACPI device discovery with proper IOACPIPlatformDevice handling
    IOACPIPlatformDevice *acpiDev = nullptr;
    const char* targetADR = "0x00020000"; // GFX0 _ADR value

    // Primary ACPI walk with enhanced type safety
    IOLog("🧭 Starting ACPI plane walk from PCI device\n");
    IORegistryEntry *acpiWalker = pciDevice;
    while ((acpiWalker = acpiWalker->getParentEntry(gIOACPIPlane)) != nullptr) {
        // Safe name and location extraction
        const char* name = acpiWalker->getName() ? acpiWalker->getName() : "unnamed";
        const char* location = acpiWalker->getLocation() ? acpiWalker->getLocation() : "no-location";
        IOLog(" → Visiting ACPI node: %s @ %s\n", name, location);

        // Check if this is actually an IOACPIPlatformDevice
        IOACPIPlatformDevice *platformDev = OSDynamicCast(IOACPIPlatformDevice, acpiWalker);
        if (!platformDev) {
            IOLog("   |_ Not an IOACPIPlatformDevice, skipping\n");
            continue;
        }

        // Safe _ADR checking
        OSData* adr = OSDynamicCast(OSData, platformDev->getProperty("_ADR"));
        if (adr && adr->getLength() == sizeof(uint32_t)) {
            uint32_t adrVal = 0;
            bcopy(adr->getBytesNoCopy(), &adrVal, sizeof(adrVal));
            IOLog("   |_ _ADR = 0x%08X\n", adrVal);
            
            if (adrVal == 0x00020000) {
                IOLog("✅ Found matching _ADR (0x00020000) - potential GFX0\n");
                acpiDev = platformDev;
                acpiDev->retain(); // Take ownership
                break;
            }
        }
    }

    
     // Fallback paths with proper type casting
    if (!acpiDev) {
        IOLog("🔍 Trying fallback paths to locate GFX0\n");
        
        // Array of possible GFX0 paths to try
        const char* gfx0Paths[] = {
            "/_SB/PC00/GFX0",
            "/_SB/PCI0/GFX0",
            "/_SB/GFX0",
            nullptr // Sentinel
        };

        for (int i = 0; gfx0Paths[i] != nullptr; i++) {
            IORegistryEntry *gfx0Entry = IORegistryEntry::fromPath(gfx0Paths[i], gIOACPIPlane);
            if (!gfx0Entry) continue;
            
            // Proper type casting check
            IOACPIPlatformDevice *platformDev = OSDynamicCast(IOACPIPlatformDevice, gfx0Entry);
            if (platformDev) {
                IOLog("✅ Found GFX0 at path: %s\n", gfx0Paths[i]);
                acpiDev = platformDev;
                acpiDev->retain(); // Take ownership
                gfx0Entry->release(); // Release the intermediate object
                break;
            }
            gfx0Entry->release(); // Clean up if cast failed
        }
    }
    
    
    
    // Final ACPI device validation and _DSM evaluation
    if (acpiDev) {
        const char* acpiName = acpiDev->getName();
        IOLog("✅ Successfully located ACPI parent: %s\n", acpiName ? acpiName : "unnamed");

        // --- _DSM Evaluation with Enhanced Safety ---
        uint8_t dsmUUID[16] = {
            0xA0, 0x12, 0x93, 0x6E, 0x50, 0x9A, 0x4C, 0x5B,
            0x8A, 0x21, 0x3A, 0x36, 0x15, 0x29, 0x2C, 0x79
        };

        OSObject *dsmParams[4] = { nullptr };
        bool dsmSuccess = false;

        do { // Error handling scope
            // Create UUID parameter
            dsmParams[0] = OSData::withBytes(dsmUUID, sizeof(dsmUUID));
            if (!dsmParams[0]) {
                IOLog("❌ Failed to create UUID parameter\n");
                break;
            }

            // Create revision parameter
            dsmParams[1] = OSNumber::withNumber(0ULL, 32);
            if (!dsmParams[1]) {
                IOLog("❌ Failed to create revision parameter\n");
                break;
            }

            // Create function index parameter
            dsmParams[2] = OSNumber::withNumber(1ULL, 32);
            if (!dsmParams[2]) {
                IOLog("❌ Failed to create function index\n");
                break;
            }

            // Create empty package parameter
            dsmParams[3] = OSArray::withCapacity(0);
            if (!dsmParams[3]) {
                IOLog("❌ Failed to create package parameter\n");
                break;
            }

     
        
            // Evaluate _DSM - now properly on IOACPIPlatformDevice
            OSObject *dsmResult = nullptr;
            IOReturn ret = acpiDev->evaluateObject("_DSM", &dsmResult, dsmParams, 4);
            
            if (ret == kIOReturnSuccess && dsmResult) {
                IOLog("✅ _DSM evaluation succeeded (%s)\n",
                      dsmResult->getMetaClass()->getClassName());
                dsmResult->release();
                dsmSuccess = true;
            } else {
                IOLog("❌ _DSM evaluation failed (0x%x)\n", ret);
            }
        } while (false);

        // Cleanup parameters
        for (int i = 0; i < 4; i++) {
            if (dsmParams[i]) dsmParams[i]->release();
        }

        if (!dsmSuccess) {
            IOLog("⚠️ _DSM method did not execute successfully\n");
        }

        acpiDev->release(); // Release our retained reference
    } else {
        IOLog("❌ Failed to locate GFX0 ACPI device\n");
        setProperty("ACPI-Status", "GFX0-not-found");
    }
    */
    
    
    pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!pciDevice) {
        IOLog("❌ Provider is not IOPCIDevice\n");
        return false;
    }

    //    pciDevice->retain();

    
    
    // 1️⃣ Open PCI device
    IOLog("📦 Opening PCI device...\n");
    if (!pciDevice->open(this)) {
        IOLog("❌ Failed to open PCI device\n");
            return false;
    }

  
    
    
  //  IOLog("⚠️ Skipping enablePCIPowerManagement (causes freeze on some systems)\n");
  // 2️⃣ Optional: PCI Power Management (safe here)
    IOLog("⚡️ Powering up PCI device...\n");
    if (pciDevice->hasPCIPowerManagement()) {
        IOLog("Using modern power management\n");
        pciDevice->enablePCIPowerManagement(kPCIPMCSPowerStateD0);
    }
    IOSleep(100);

    
    
    
    //verify BAR0 satus
    IOLog("veryfying bar0 adddress");
    uint32_t bar0=pciDevice->configRead32(kIOPCIConfigBaseAddress0);
    IOLog("PCI BAR0 = 0x%08X\n",bar0);
    
    
    if ((bar0 & ~0xf)==0){
        
        IOLog("bar0 invalid, device not assigned memory");

        return false;
    }
    
    
    
    // --- Store physical BAR0 base address ---
    uint32_t bar0Low  = pciDevice->configRead32(kIOPCIConfigBaseAddress0) & ~0xF;
    uint32_t bar0High = pciDevice->configRead32(kIOPCIConfigBaseAddress0 + 4);

    bar0Phys = ((uint64_t)bar0High << 32) | bar0Low;

    IOLog("📌 BAR0 physical address = 0x%llX\n", (unsigned long long)bar0Phys);

    
    
    
    // 3️⃣ Enable PCI Memory and IO
    IOLog("🛠 Enabling PCI memory and IO...\n");
    pciDevice->setMemoryEnable(true);
    pciDevice->setIOEnable(false);
    IOSleep(10); // Let it propagate


    // 4️⃣ Confirm enablement via config space
    uint16_t command = pciDevice->configRead16(kIOPCIConfigCommand);
    bool memEnabled = command & kIOPCICommandMemorySpace;
    bool ioEnabled  = command & kIOPCICommandIOSpace;
    if (!memEnabled) {
        IOLog("❌ Resource enable failed (PCI command: 0x%04X, mem:%d, io:%d)\n", command, memEnabled, ioEnabled);
        return false;
    }
    IOLog("✅ PCI resource enable succeeded (command: 0x%04X)\n", command);

    
    
    

    IOLog("About to Map Bar0");
    // 5️⃣ MMIO BAR0 mapping
    if (pciDevice->getDeviceMemoryCount() < 1) {
        IOLog("❌ No MMIO regions available\n");
        return false;
    }

    mmioMap = pciDevice->mapDeviceMemoryWithIndex(0);
    if (!mmioMap || mmioMap->getLength() < 0x100000) {
        IOLog("❌ BAR0 mapping failed or too small\n");
        OSSafeReleaseNULL(mmioMap);
        return false;
    }
    mmioBase = (volatile uint8_t*)mmioMap->getVirtualAddress();
    IOLog("✅ BAR0 mapped successfully (len: 0x%llX)\n", mmioMap->getLength());

    // ❌⛔️ NEVER TOUCH MMIO YET – GPU NOT READY!

    
    
    
   
    // 6️⃣ Power management: wake up GPU
        IOLog("🔌 Calling initPowerManagement()...\n");
        
        if (!initPowerManagement()) {
            IOLog("❌ FATAL: initPowerManagement failed (Reported Failure). GPU is not awake.");
            IOLog("Aborting start() to prevent system freeze.");
            
            mmioMap->release();
            mmioMap = nullptr;
            pciDevice->close(this);
            pciDevice->release();
            pciDevice = nullptr;
            
            return false;
        }
        
        IOLog("✅ initPowerManagement() (Reported Success). Trust, but verify...\n");

      
    
    
    
    
    
    
    
    // --- NEW: TRUST BUT VERIFY (Safe) ---
    uint32_t gt_status = safeMMIORead(0x13805C);
    uint32_t forcewake_ack = safeMMIORead(0x130044);

    if ((gt_status == 0x0) || ((forcewake_ack & 0xF) == 0x0)) {
        IOLog("⚠️ GPU verification failed: GT_STATUS=0x%08X, ACK=0x%08X — still waking up\n", gt_status, forcewake_ack);
        IOLog("Releasing PCI + MMIO resources safely.\n");

        if (mmioMap) { mmioMap->release(); mmioMap = nullptr; }
        if (pciDevice) {
            pciDevice->close(this);
            pciDevice->release();
            pciDevice = nullptr;
        }

        return false; // Exit gracefully (prevent freeze)
    }

    IOLog("✅ GPU verified awake: GT_STATUS=0x%08X, ACK=0x%08X\n", gt_status, forcewake_ack);


    
    
    
    
    
        // 7️⃣ Now it's SAFE to do MMIO read/write
        // We already read pciID, so let's check the other registers
        IOLog("FORCEWAKE_ACK: 0x%08X\n", safeMMIORead(0xA188)); // Read the register we just polled
    IOLog("✅ Returned from initPowerManagement()\n");
   
    
    
    
    
    
    
    
    // 7️⃣ Now it's SAFE to do MMIO read/write
    uint32_t zeroReg = safeMMIORead(0x0000);
    IOLog("MMIO[0x0000] = 0x%08X\n", zeroReg);

    uint32_t ack = safeMMIORead(0xA188);
    IOLog("FORCEWAKE_ACK: 0x%08X\n", ack);

    
    
    
    // 8️⃣ Optional: MMIO register dump
    IOLog("🔍 MMIO Register Dump:\n");
    for (uint32_t offset = 0; offset < 0x40; offset += 4) {
        uint32_t val = safeMMIORead(offset);
        IOLog("[0x%04X] = 0x%08X\n", offset, val);
    }

    
    
    
    bar0Map = pciDevice->mapDeviceMemoryWithIndex(0);
    mmioBase = (volatile uint8_t*) bar0Map->getVirtualAddress();
    
    
    
    
/*
    // === GPU Acceleration Properties ===
    {
        // Required properties for Quartz Extreme / Core Animation
               OSArray* accelTypes = OSArray::withCapacity(4); // Increased capacity for more types
               if (accelTypes) {
                   accelTypes->setObject(OSSymbol::withCString("Accel"));
                   accelTypes->setObject(OSSymbol::withCString("Metal"));
                   accelTypes->setObject(OSSymbol::withCString("OpenGL"));
                   accelTypes->setObject(OSSymbol::withCString("Quartz"));
                   setProperty("IOAccelTypes", accelTypes);
                   accelTypes->release();
                   IOLog("GPU Acceleration Properties used\n"); // Added newline for cleaner log
               }
    }
  */

    
    //display bounds
    OSDictionary* bounds = OSDictionary::withCapacity(2);
    bounds->setObject("Height", OSNumber::withNumber(1080, 32));
    bounds->setObject("Width", OSNumber::withNumber(1920, 32));
    setProperty("IOFramebufferBounds", bounds);
    bounds->release();

    
    
    
    
    
    
    
    const uint32_t width  = 1920;
    const uint32_t height = 1080;
    const uint32_t bpp    = 4;

    uint32_t rawSize     = width * height * bpp;
    uint32_t alignedSize = (rawSize + 0xFFFF) & ~0xFFFF; // 64KB aligned

    IOLog("🧠 Allocating framebuffer memory: %ux%u = %u bytes (aligned to 0x%X)\n",
          width, height, rawSize, alignedSize);

    int retries = 3;

    while (retries-- > 0) {

        framebufferMemory = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task,
            kIODirectionInOut | kIOMemoryKernelUserShared,
            alignedSize,
            0x000000003FFFF000ULL   // BELOW 1GB, 4KB aligned, macOS-friendly
        );


        if (!framebufferMemory) {
            IOLog("❌ Failed to allocate framebuffer descriptor (retry %d)\n", retries);
            continue;
        }
        
        if (framebufferMemory->prepare() != kIOReturnSuccess) {
            IOLog("❌ framebufferMemory->prepare() failed\n");
            framebufferMemory->release();
            framebufferMemory = nullptr;
            continue;
        }

        break;
    }

    
    
    IOPhysicalAddress fbPhys = framebufferMemory->getPhysicalAddress();

    if ((fbPhys & 0xFFFF) != 0) {
        IOLog(" FB not 64KB aligned, but OK — GGTT mapping handles alignment\n");
    }
    else
    { IOLog("64 KB Aligned");
        
    }
    
    
    void* fbAddr = framebufferMemory->getBytesNoCopy();
    if (fbAddr) bzero(fbAddr, rawSize);

    //IOPhysicalAddress fbPhys = framebufferMemory->getPhysicalAddress();
    size_t fbLen = framebufferMemory->getLength();

    this->kernelFBPtr  = fbAddr;
    this->kernelFBSize = fbLen;
    this->kernelFBPhys = fbPhys;

    IOLog("📦 Final FB physical address: 0x%08llX\n", fbPhys);
    IOLog("📏 Final FB length: 0x%08zX\n", fbLen);

    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    // Optional surface descriptor (for later IOSurface/Metal integration)
    framebufferSurface = IOMemoryDescriptor::withAddressRange(
        fbPhys,
        fbLen,
        kIODirectionInOut,
        kernel_task
    );

    if (framebufferSurface) {
        IOLog("✅ Framebuffer surface registered\n");
    } else {
        IOLog("❌ Failed to create framebuffer surface\n");
    }

    
    
    
    if (kernelFBPtr) {
        uint32_t *px = (uint32_t *)kernelFBPtr;
        size_t pixels = (1920 * 1080);
        for (size_t i = 0; i < pixels; i++) {
            px[i] = 0xFF0000FF; // ARGB = solid red (A=255, R=0, G=0, B=255)
        }
        IOLog("🟥 Filled framebuffer with test color\n");
    }

    
    
    
    
    // Create work loop and command gate
    workLoop = IOWorkLoop::workLoop();
    if (!workLoop) {
        IOLog("❌ Failed to create work loop\n");
        return false;
    }

       
       commandGate = IOCommandGate::commandGate(this);
       if (!commandGate || workLoop->addEventSource(commandGate) != kIOReturnSuccess) {
           IOLog("Failed to create command gate\n");
           return false;
       }
    
    
/*
    // create timer event source and add to workloop
    if (!fWorkLoop) {
        fWorkLoop = getWorkLoop();
    }
    if (fWorkLoop) {
        fVBlankTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &FakeIrisXEFramebuffer::vblankTick));
        if (fVBlankTimer) {
            fWorkLoop->addEventSource(fVBlankTimer);
            // schedule first tick after 16 ms
            fVBlankTimer->setTimeoutMS(16);
        }
    }
*/
    
    
    
    
    cursorMemory = IOBufferMemoryDescriptor::withOptions(
        kIOMemoryKernelUserShared | kIODirectionInOut,
        4096,  // 4KB for cursor
        page_size
    );
    if (cursorMemory) {
        bzero(cursorMemory->getBytesNoCopy(), 4096);
        IOLog("Cursor memory allocated\n");
    } else {
        IOLog("Failed to allocate cursor memory\n");
    }
    
    

    // === Dummy EDID ===
    {
        static const uint8_t fakeEDID[128] = {
            0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
            0x4C, 0x83, 0x40, 0x56, 0x01, 0x01, 0x01, 0x01,
            0x0D, 0x1A, 0x01, 0x03, 0x80, 0x30, 0x1B, 0x78,
            0x0A, 0xEE, 0x95, 0xA3, 0x54, 0x4C, 0x99, 0x26,
            0x0F, 0x50, 0x54, 0x00, 0x00, 0x00, 0x01, 0x01,
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0x01, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38,
            0x2D, 0x40, 0x58, 0x2C, 0x45, 0x00, 0x13, 0x2A,
            0x21, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0xFD,
            0x00, 0x38, 0x4B, 0x1E, 0x51, 0x11, 0x00, 0x0A,
            0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00,
            0x00, 0xFC, 0x00, 0x46, 0x61, 0x6B, 0x65, 0x20,
            0x44, 0x69, 0x73, 0x70, 0x6C, 0x61, 0x79, 0x0A,
            0x00, 0x00, 0x00, 0xFF, 0x00, 0x31, 0x32, 0x33,
            0x34, 0x35, 0x36, 0x0A, 0x20, 0x20, 0x20, 0x20,
            0x20, 0x20, 0x01, 0x55
        };
        OSData *edidData = OSData::withBytes(fakeEDID, sizeof(fakeEDID));
        if (edidData) {
            setProperty("IODisplayEDID", OSData::withBytes(fakeEDID, 128));
            edidData->release();
            IOLog("📺 Fake EDID published\n");
        }

        setProperty("IOFBHasPreferredEDID", kOSBooleanTrue);
    }

    

    // Display Timing Information

    const uint8_t timingData[] = {
        0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,  // Header
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Serial
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Basic params
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Detailed timings
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // Extension blocks
    };

    OSData* timingInfo = OSData::withBytes(timingData, sizeof(timingData));
    if (timingInfo) {
        setProperty("IOTimingInformation", timingInfo);
        timingInfo->release();
        IOLog("Added timing information\n");
    }
    
    
    
    timerLock = IOLockAlloc();
    if (!timerLock) {
        IOLog("❌ Failed to allocate timerLock\n");
        return false;
    }

    // Setup vsyncTimer for screen refresh (simulation only)
    if (workLoop && !isInactive()) {  // Add safety check
        vsyncTimer = IOTimerEventSource::timerEventSource(
            this,
            OSMemberFunctionCast(IOTimerEventSource::Action, this, &FakeIrisXEFramebuffer::vsyncTimerFired)
        );
        if (vsyncTimer) {
            if (workLoop->addEventSource(vsyncTimer) == kIOReturnSuccess) {
                vsyncTimer->setTimeoutMS(16);
            } else {
                vsyncTimer->release();
                vsyncTimer = nullptr;
            }
        }
    }
    
 
 
      
    OSDictionary* displayInfo = OSDictionary::withCapacity(1);
    OSDictionary* brightness = OSDictionary::withCapacity(1);
    brightness->setObject("min", OSNumber::withNumber(10, 32));
    brightness->setObject("max", OSNumber::withNumber(255, 32));
    displayInfo->setObject("brightness", brightness);
    setProperty("IODisplayParameters", displayInfo);

    setProperty("default-width", 1920, 32);
    setProperty("default-height", 1080, 32);
    
    setProperty("IOFBDepth", 32, 32); // For ARGB8888

    
    setProperty("IOFBHas3DAccelerator", kOSBooleanTrue);
    setProperty("IOFramebufferPrefersWG", kOSBooleanTrue); // optional
    setProperty("IOFBSystemPowerProfile", OSString::withCString("Full"));

    setProperty("IOAccelRevision", 2, 32);
    setProperty("IOAccelVRAMSize", framebufferMemory->getLength(), 64);    setProperty("IOFBNeedsRefresh", kOSBooleanFalse);
    setProperty("IOFBHostAccessFlags", (uintptr_t)0x1); // host write allowed

    setProperty("IOFBUserClientClass", OSSymbol::withCString("IOFramebufferUserClient"));

    setProperty("IOFBSharedUserClient", kOSBooleanTrue);
    setProperty("IOFramebufferSharedUserClient", kOSBooleanTrue);

    
    setProperty(kIOFBConsoleKey, kOSBooleanTrue);               // IOFramebufferIsConsole
    setProperty(kIOConsoleSafeBoot, kOSBooleanTrue);            // IOConsoleSafe
    setProperty(kIOKitConsoleSecurityKey, kOSBooleanTrue);      // IOKitConsoleSecurity

    setProperty("AAPL,aux-power-connected", kOSBooleanTrue);

    setProperty("IOFBIsMainDisplay", kOSBooleanTrue);
    setProperty("IOFBWidth", 1920ULL);
    setProperty("IOFBHeight", 1080ULL);
    setProperty("IOFBPixelFormat", "AR24"); // or "ARGB8888"

    
    setProperty("IOFBConfig", 1, 32);
    setProperty("IOFBDisplayModeID", OSNumber::withNumber((UInt64)0, 32)); // Default mode
    setProperty("IOFBStartupModeTimingID", OSNumber::withNumber((UInt64)0, 32));
     
     
    setProperty("AAPL,boot-display", kOSBooleanTrue);

    setProperty("IOFramebufferOpenGLIndex", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFramebufferSharedIndex", OSNumber::withNumber((UInt64)0, 32));

    
    setProperty("IOFBAccelerator", kOSBooleanTrue);
    setProperty("IOFBDependentIndex",OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBCursorInfo", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBTransform", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBGammaHeaderSize", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBWaitCursorFrames", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBClientConnectIndex", 1, 32);
    setProperty("IOFBCursorSupported", kOSBooleanFalse);
    setProperty(kIOFBHardwareCursorSupportedKey, kOSBooleanFalse);
        
    setProperty(kIOFBScalerInfoKey, 0ULL, 32);
    setProperty(kIOFBDisplayModeCountKey, static_cast<UInt64>(1), 32);
    setProperty("IOFBCurrentPixelCount", 1920ULL * 1080ULL, 128);
    setProperty("IOFBCursorScale", 0x10000, 32); // 1.0 fixed-point
    setProperty("IOFBDisplayCount", (uint32_t)1);
    setProperty("IOFBVerbose", kOSBooleanTrue);
    setProperty("IOFBTranslucencySupport", kOSBooleanTrue);
    setProperty("IOSupportsCLUTs", kOSBooleanTrue);
    setProperty("AAPL,HasPanel", kOSBooleanTrue);
    setProperty("built-in", kOSBooleanTrue);
    setProperty("AAPL,gray-page", kOSBooleanTrue);
    setProperty("IOFBTransparency", kOSBooleanTrue);
    setProperty("IOSurfaceSupport", kOSBooleanTrue);


    setProperty("IOProviderClass", "IOPCIDevice");
  
    /*
    setProperty("MetalPluginName", OSSymbol::withCString("AppleIntelICLLPGraphicsMTLDriver"));
    setProperty("MetalPluginClassName", OSSymbol::withCString("AppleIntelICLLPGraphicsMTLDriver")); // Should match MetalPluginName
    setProperty("MetalPluginVersion", OSNumber::withNumber(120ULL, 32));
    setProperty("MetalFeatures", OSNumber::withNumber(0xFFFFFFFFULL, 32)); // Use ULL for consistency
      */
    
    
    getProvider()->setProperty("VRAM,totalsize", framebufferMemory->getLength(), 64);
    
    setProperty("IOFBConnectFlags", kIOConnectionBuiltIn, 32);
    setProperty("IOFBCurrentConnection", OSNumber::withNumber((UInt64)0, 32));
    setProperty("model", OSData::withBytes("Intel Iris Xe Graphics", 22));
    setProperty("IOFramebufferDisplayIndex", (uint32_t)0);
    setProperty("IOFBStartupDisplayModeTimingID", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFramebufferDisplay", kOSBooleanTrue);     // Very important

    
    setProperty(kIOConsoleDeviceKey, kOSBooleanTrue);

    setProperty("IOConsoleFramebuffer", kOSBooleanTrue);

    
    
    
    setProperty("IOConsole", kOSBooleanTrue);
    setProperty("IOFBConsole", 1,32);
    setProperty("IOFBConsoleVisible", kOSBooleanTrue);
    setProperty("IOConsoleLocked", kOSBooleanFalse);
    
    setProperty(kIOFBScalerInfoKey, 0ULL, 32);
    setProperty(kIOFBDisplayModeCountKey, 1ULL, 32);
    setProperty("IOFBCurrentPixelCount", 1920ULL * 1080ULL, 128);
    setProperty("IOFBWidth", 1920, 32);
    setProperty("IOFBHeight", 1080, 32);
    setProperty("IOFBBytesPerRow", 1920 * 4, 32);
    setProperty("IOFBMemorySize", framebufferMemory->getLength(), 32);

    
    setProperty("IOFBGammaWidth", 8, 32);
    setProperty("IOFBGammaCount", 256, 32);
    setProperty("IOFBGammaChannelCount", 3, 32);

    
    setProperty("IOFramebuffer", kOSBooleanTrue);

    
    
    OSDictionary* fbInfo = OSDictionary::withCapacity(1);
    fbInfo->setObject("FramebufferType", OSSymbol::withCString("IntelIrisXe"));
    setProperty("IOFramebufferInformation", fbInfo);
    fbInfo->release();

    
    
    
    OSArray* pixelFormats = OSArray::withCapacity(2);
    pixelFormats->setObject(OSData::withBytes("AR24", 4)); // ARGB8888
    pixelFormats->setObject(OSData::withBytes("BG24", 4)); // BGRA8888
    setProperty("IOFBSupportedPixelFormats", pixelFormats);
    pixelFormats->release();

    

    // Cursor
    
    IOBufferMemoryDescriptor* cursorMemory = IOBufferMemoryDescriptor::inTaskWithOptions(
        kernel_task,
        kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        64 * 1024,
        page_size
    );
    if (cursorMemory) {
        setProperty("IOFBCursorMemory", cursorMemory);
        cursorMemory->release();
    }

    
    
    OSNumber* cursorSizeNum = OSNumber::withNumber(32ULL, 32); // Renamed variable to avoid conflict
       if (cursorSizeNum) {
           OSObject* values[1] = { cursorSizeNum };
           OSArray* array = OSArray::withObjects((const OSObject**)values, 1);
           if (array) {
               setProperty("IOFBCursorSizes", array);
               array->release();
           }
           cursorSizeNum->release();
       }
  
    
    
    

    setProperty("IOFBNumberOfConnections", 1, 32);
    setProperty("IOFBConnectionFlags", OSNumber::withNumber(0x100, 32));
    setProperty("IOConsoleMode", kOSBooleanTrue); // Use mode 0 as default
    

    setProperty("IOFBOnline", kOSBooleanTrue);
    setProperty("IOFBIsConsoleDevice", kOSBooleanTrue);     // safer than `true`
    setProperty("IOConsoleSafe", kOSBooleanTrue);           // optional but helps
    setProperty("IOFBIsConnectionReady", kOSBooleanTrue);

    setProperty("IOFBMemoryCount", 2, 32);  // VRAM + Cursor memory
    setProperty("IOFBVRAMSize", framebufferMemory->getLength(), 32);
    setProperty("IOFBRowBytes", 1920 * 4, 32);
    setProperty(kIOFBNeedsRefreshKey, kOSBooleanFalse);
    setProperty("IOFBBytesPerPixel", (UInt32)4);



    
    IORegistryEntry* display = IORegistryEntry::fromPath("IOService:/IOResources/IOBacklightDisplay");
    if (display) {
        display->setProperty("IOBacklightDisplay", kOSBooleanFalse);
        display->release();
        IOLog("Forced display wake\n");
    }

    
    
    
    
    
    
    //real display mode dictionary
    OSDictionary* displayDict = OSDictionary::withCapacity(1);
    displayDict->setObject("IODisplayConnectFlags", OSNumber::withNumber((uint64_t)0, 32));
    setProperty("display0", displayDict);
    displayDict->release();
    
    
    OSDictionary* modeInfo = OSDictionary::withCapacity(4);
    modeInfo->setObject(kIOFBWidthKey, OSNumber::withNumber(1920, 32));
    modeInfo->setObject(kIOFBHeightKey, OSNumber::withNumber(1080, 32));
    modeInfo->setObject(kIOFBRefreshRateKey, OSNumber::withNumber(60 << 16, 32)); // 60Hz fixed-point
    modeInfo->setObject(kIOFBFlagsKey, OSNumber::withNumber((uint64_t)0, 32));
    OSDictionary* allModes = OSDictionary::withCapacity(1);
    allModes->setObject("0", modeInfo);
    IOLog("real display mode dictionary used");
    modeInfo->release();
    setProperty("IOFBDisplayModeInformation", allModes);
    allModes->release();
       
    
    
    setNumberOfDisplays(1);
    setDisplayMode(0, 0);

    
    
    
    
    fullyInitialized = true;
    driverActive = true;

    
    bzero(framebufferMemory->getBytesNoCopy(), framebufferMemory->getLength());

    
    
    workLoop = getWorkLoop();
    if (workLoop) {
        workLoop->retain();
        IOLog("✅ Workloop acquired\n");

        IOTimerEventSource* activateTimer = IOTimerEventSource::timerEventSource(
            this,
            [](OSObject* owner, IOTimerEventSource* timer) {
                auto fb = OSDynamicCast(FakeIrisXEFramebuffer, owner);
                if (fb) {
                    IOLog("🔥 Timer fired, calling enableController()\n");
                    fb->activatePowerAndController();
                }
            }
        );

        if (activateTimer) {
            workLoop->addEventSource(activateTimer);
            activateTimer->setTimeoutMS(5000); // 5 sec delay
            IOLog("⏰ Timer scheduled for 5s\n");
        }
    } else {
        IOLog("❌ No valid workloop - timer not scheduled\n");
    }

    

    setProperty(kIOConsoleFramebufferKey, kOSBooleanTrue);
    setProperty(kIOFramebufferConsoleKey, kOSBooleanTrue);
    
    
    
    /*
    // 3. ✅ PE_Video console registration
    PE_Video consoleInfo;
    bzero(&consoleInfo, sizeof(consoleInfo));

    consoleInfo.v_baseAddr = (unsigned long)(framebufferMemory->getPhysicalAddress());
    if (!consoleInfo.v_baseAddr) {
        IOLog("❌ Invalid framebuffer address at PE_Video console registration\n");
        return false;
    }
    
    consoleInfo.v_display  = 3;
    consoleInfo.v_width    = 1920;
    consoleInfo.v_height   = 1080;
    consoleInfo.v_rowBytes = 1920 * 4;
    consoleInfo.v_depth    = 32;

    IOSleep(200);
    if (getPlatform()) {
        getPlatform()->setConsoleInfo(&consoleInfo, 0);
        IOLog("✅ PE_Video set: v_baseAddr=0x%lx\n", consoleInfo.v_baseAddr);
        IOSleep(200);
        deliverFramebufferNotification(0, kIOFBNotifyConsoleReady, nullptr);
        if (getProperty("IOFramebufferConsoleKey")) {
            IOLog("✅ Console registered in IORegistry\n");
        } else {
            IOLog("❌ Console not registered in IORegistry\n");
        }
    } else {
        IOLog("❌ getPlatform() returned nullptr\n");
        return false;
    }
    
    
    IOLog("🖥️  PE_Video initialized: %lux%lu stride=%lu depth=%lu addr=0x%lx\n",
          consoleInfo.v_width,
          consoleInfo.v_height,
          consoleInfo.v_rowBytes,
          consoleInfo.v_depth,
          consoleInfo.v_baseAddr);
    
    
     */

    
    IOService::getPlatform()->setProperty("IOFramebufferConsoleKey", kOSBooleanTrue);
    
    
    publishDisplay();

    // 6. Then flush and notify
    flushDisplay();

    
    
    
    // 5. Notify WindowServer
    deliverFramebufferNotification(0, kIOFBNotifyWillPowerOn, nullptr);
    deliverFramebufferNotification(0, kIOFBNotifyDidPowerOn, nullptr);
    deliverFramebufferNotification(0, 0x10, nullptr);      // display mode set complete
    deliverFramebufferNotification(0, 'dmod', nullptr);    // publish mode
    deliverFramebufferNotification(0, kIOFBNotifyDisplayModeWillChange, nullptr);
    deliverFramebufferNotification(0, kIOFBNotifyDisplayModeDidChange, nullptr);
    
    
    
    
    /*
    //debug verification
    IOLog("===== CONSOLE STATUS VERIFICATION ====\n");
    IOLog("isConsoleDevice(): %s\n", isConsoleDevice() ? "YES" : "NO");
    IOLog("IOFramebufferConsoleKey: %s\n",
          getProperty(kIOFramebufferConsoleKey) ? "SET" : "MISSING");

    IORegistryEntry* console = IORegistryEntry::fromPath("/IOConsole", gIOServicePlane);
    if (console) {
        IOLog("Current console: %s\n", console->getName());
        console->release();
    } else {
        IOLog("No console registered in IORegistry!\n");
    }
    IOLog("  🔎 After forcing console props: IOConsoleDevice=%s\n",
          getProperty("IOConsoleDevice") ? "true" : "false");
    
    
    
    IORegistryEntry *wranglerEntry = IORegistryEntry::fromPath("/IOResources/IODisplayWrangler");
    if (wranglerEntry) {
        IOService *wrangler = OSDynamicCast(IOService, wranglerEntry);
        if (wrangler) {
            IOLog("(FakeIrisXEFramebuffer) Forcing attach to IODisplayWrangler and promoting as console\n");

            // Safely attach to the display wrangler hierarchy
            this->attachToParent(wrangler, gIOServicePlane);

            // Promote to console framebuffer
            wrangler->setProperty("IOFramebuffer", kOSBooleanTrue);
            setProperty("IOFramebufferIsConsole", kOSBooleanTrue);
            setProperty("IOConsoleDevice", kOSBooleanTrue);
        }
        wranglerEntry->release();
    }

*/
    
    
    
    IOLog("---- PCI BAR DUMP ----\n");

    uint32_t count = pciDevice->getDeviceMemoryCount();
    IOLog("DeviceMemoryCount = %u\n", count);

    for (uint32_t i = 0; i < count; i++) {
        IODeviceMemory* mem = pciDevice->getDeviceMemoryWithIndex(i);
        if (!mem) continue;

        IOPhysicalAddress phys = mem->getPhysicalAddress();
        uint64_t length = mem->getLength();

        IOLog("BAR %u: phys=0x%llX len=0x%llX\n",
              i,
              (unsigned long long)phys,
              (unsigned long long)length);
    }

    

    
    
    IOLog("---- PCI GTTMMADR dump ----\n");

    IOLog("PCI GTTMMADR low = 0x%08X\n", pciDevice->configRead32(0x18));
    IOLog("PCI GTTMMADR hi  = 0x%08X\n", pciDevice->configRead32(0x1C));

    
    
    // 6. Finally, publish the framebuffer
    attachToParent(getProvider(), gIOServicePlane);
    
    
    registerService();
    IOLog("register service called");

    
    
    
    // Clean up temporary OSObjects
    if (displayInfo) displayInfo->release();
    if (brightness)   brightness->release();
    
    IOLog("🏁 FakeIrisXEFramebuffer::start() - Completed\n");
    return true;

}



void FakeIrisXEFramebuffer::stop(IOService* provider)
{
    IOLog("FakeIrisXEFramebuffer::stop() called — scheduling gated cleanup\n");

    // Mark we are shutting down so any timers/work will early-exit
    IOLockLock(timerLock);
    driverActive = false;
    shuttingDown = true;
    IOLockUnlock(timerLock);

    
    if (commandGate) {
        // runAction will call staticStopAction() on the gated thread synchronously.
        commandGate->runAction(&FakeIrisXEFramebuffer::staticStopAction);
    } else {
        // fallback: if no gate exists, do best-effort cleanup inline
        performSafeStop();
    }

    // Now call superclass stop after gated cleanup.
    super::stop(provider);
}



IOReturn FakeIrisXEFramebuffer::staticStopAction(OSObject *owner,
                                                 void * /*arg0*/,
                                                 void * /*arg1*/,
                                                 void * /*arg2*/,
                                                 void * /*arg3*/)
{
    FakeIrisXEFramebuffer *fb = OSDynamicCast(FakeIrisXEFramebuffer, owner);
    if (!fb) return kIOReturnBadArgument;

    IOLog("FakeIrisXEFramebuffer::staticStopAction() running on gated thread\n");
    fb->performSafeStop();
    return kIOReturnSuccess;
}

void FakeIrisXEFramebuffer::performSafeStop()
{
    IOLog("FakeIrisXEFramebuffer::performSafeStop() — doing gated cleanup\n");

    // Cancel timers and remove event sources under workloop/gate
    if (vsyncTimer) {
        vsyncTimer->cancelTimeout();
        if (workLoop) {
            workLoop->removeEventSource(vsyncTimer);
        }
        vsyncTimer->release();
        vsyncTimer = nullptr;
    }

    if (displayInjectTimer) {
        displayInjectTimer->cancelTimeout();
        if (workLoop) {
            workLoop->removeEventSource(displayInjectTimer);
        }
        displayInjectTimer->release();
        displayInjectTimer = nullptr;
    }

    if (commandGate && workLoop) {
        // commandGate is still valid here — do not remove it yet because we're running inside it.
        // We will remove it after clearing other sources (below) from the workloop.
        // (But do not call workLoop->removeEventSource(commandGate) here; do it after the gated cleanup returns.)
    }

    // Stop power management (PM) under gated thread
    PMstop();

    // Release GPU resources and memory descriptors (these touch IOGraphics/IOBuffer objects)
    OSSafeReleaseNULL(framebufferMemory);
    OSSafeReleaseNULL(framebufferSurface);
    OSSafeReleaseNULL(cursorMemory);
    OSSafeReleaseNULL(mmioMap);
    OSSafeReleaseNULL(vramMemory);

    // Remove interrupt sources if any
    if (vsyncSource && workLoop) {
        workLoop->removeEventSource(vsyncSource);
        vsyncSource->release();
        vsyncSource = nullptr;
    }

    // Now remove and release the commandGate and workLoop safely (still on the workloop/gated thread)
    if (commandGate && workLoop) {
        workLoop->removeEventSource(commandGate);
        commandGate->release();
        commandGate = nullptr;
    }

    if (workLoop) {
        workLoop->release();
        workLoop = nullptr;
    }

    // Free other locks and arrays
    if (timerLock) {
        IOLockFree(timerLock);
        timerLock = nullptr;
    }
    if (powerLock) {
        IOLockFree(powerLock);
        powerLock = nullptr;
    }

    if (interruptList) {
        interruptList->release();
        interruptList = nullptr;
    }

    // Close PCI device and release provider only after gated cleanup
    if (pciDevice) {
        pciDevice->close(this);
        pciDevice->release();
        pciDevice = nullptr;
    }

    IOLog("FakeIrisXEFramebuffer::performSafeStop() — gated cleanup complete\n");
}














void FakeIrisXEFramebuffer::startIOFB() {
    IOLog("FakeIrisXEFramebuffer::startIOFB() called\n");
    deliverFramebufferNotification(0, kIOFBNotifyDisplayModeChange, nullptr); // This is enough

}

 
 
void FakeIrisXEFramebuffer::free() {
    IOLog("FakeIrisXEFramebuffer::free() called\n");
    
    // ✅ CLEANUP ALL ALLOCATED RESOURCES
    if (clutTable) {
        IOFree(clutTable, 256 * sizeof(IOColorEntry));
        clutTable = nullptr;
    }
    
    if (gammaTable) {
        IOFree(gammaTable, gammaTableSize);
        gammaTable = nullptr;
        gammaTableSize = 0;
    }
    
    if (interruptList) {
        // Cleanup all interrupt registrations
        for (unsigned int i = 0; i < interruptList->getCount(); i++) {
            OSData* data = OSDynamicCast(OSData, interruptList->getObject(i));
            if (data) {
                IOFree((void*)data->getBytesNoCopy(), sizeof(InterruptInfo));
            }
        }
        interruptList->release();
        interruptList = nullptr;
    }
    
    if (powerLock) {
        IOLockFree(powerLock);
        powerLock = nullptr;
    }
    
    if (timerLock) {
        IOLockFree(timerLock);
        timerLock = nullptr;
    }
    
    driverActive = false;
    
    if (workLoop) {
        if (commandGate) {
            workLoop->removeEventSource(commandGate);
            commandGate->release();
            commandGate = nullptr;
        }
        if (vsyncTimer) {
            workLoop->removeEventSource(vsyncTimer);
            vsyncTimer->release();
            vsyncTimer = nullptr;
        }
        workLoop->release();
        workLoop = nullptr;
    }
    
    OSSafeReleaseNULL(framebufferSurface);
    OSSafeReleaseNULL(cursorMemory);
    
    super::free();
}





void* FakeIrisXEFramebuffer::getFramebufferKernelPtr() const {
    if (fFramebufferMemory)
        return fFramebufferMemory->getBytesNoCopy();
    return nullptr;
}





IOWorkLoop* FakeIrisXEFramebuffer::getWorkLoop() const {
    return workLoop;
}

bool FakeIrisXEFramebuffer::makeUsable() {
    IOLog("makeUsable() called\n");
    return super::makeUsable();
}



void FakeIrisXEFramebuffer::activatePowerAndController() {
    IOLog("⚡️ Delayed Power and Controller Activation\n");
    
    if (!pciDevice || !mmioBase) {
        IOLog("❌ activatePowerAndController: device or mmio not ready, aborting\n");
        return;
    }


    controllerEnabled = true;
    
    enableController();
    // Verify GPU still alive
        uint32_t ack = safeMMIORead(0x130044);
        IOLog("FORCEWAKE_ACK after enableController(): 0x%08X\n", ack);

    
    
    
    
    PMinit();
    
   // registerPowerDriver(this, powerStates, kNumPowerStates);
   getProvider()->joinPMtree(this);
     
   
    
    
    makeUsable();
   
    
    
    
    displayOnline = true;

    
    setProperty("IOFBOnline", true);
    setProperty("IOConsoleSafe", kOSBooleanTrue);
    
     
    IOLog("✅ Delayed power and display activation complete\n");

     
     }





bool FakeIrisXEFramebuffer::initializeHardware() {
    IOLog("initializeHardware() called\n");

    // Map MMIO space
    mmioMap = pciDevice->mapDeviceMemoryWithIndex(0);

    if (!mmioMap) {
        IOLog("Failed to map MMIO\n");
        return false;
    }
    
    mmioBase = reinterpret_cast<volatile UInt8*>(mmioMap->getVirtualAddress());

    // Initialize hardware here
    // ... GPU-specific initialization code ...
    
    return true;
}

bool FakeIrisXEFramebuffer::setupVRAM() {
    IOLog("setupVRAM() called\n");
    
    
    vramSize = 1920 * 1080 * 4;  // Ensure this is set
    vramMemory = IOBufferMemoryDescriptor::withOptions(
        kIODirectionInOut | kIOMemoryKernelUserShared,
        vramSize,
        PAGE_SIZE);
    
    if (!vramMemory) {
        IOLog("Failed to allocate VRAM\n");
        return false;
    }
    
    // Clear screen to black
       void* fbAddr = framebufferMemory->getBytesNoCopy();
       if (fbAddr) {
           memset(fbAddr, 0, framebufferMemory->getLength());
       } else {
           IOLog("❌ setupVRAM(): Failed to get framebuffer memory address.\n");
           return false;
       }
    
    
    return true;
}



IOReturn FakeIrisXEFramebuffer::staticFlushAction(OSObject *owner,
                                                  void *arg0,
                                                  void *arg1,
                                                  void *arg2,
                                                  void *arg3)
{
    FakeIrisXEFramebuffer *fb = OSDynamicCast(FakeIrisXEFramebuffer, owner);
    if (!fb) return kIOReturnBadArgument;

    fb->flushDisplay();   // safe, running on the FB workloop
    return kIOReturnSuccess;
}


void FakeIrisXEFramebuffer::scheduleFlushFromAccelerator()
{
    // Mark request
    fNeedFlush = true;

    if (commandGate) {
        // Correct runAction syntax:
        commandGate->runAction(&FakeIrisXEFramebuffer::staticFlushAction);
    } else {
        // Fallback (not recommended but safe):
        flushDisplay();
    }
}



// in FakeIrisXEFramebuffer.cpp
void FakeIrisXEFramebuffer::vblankTick(IOTimerEventSource* sender)
{
    // sequence of notifications WindowServer expects (you used these elsewhere)
    deliverFramebufferNotification(0, 0x00000006, nullptr);
    deliverFramebufferNotification(0, 0x00000008, nullptr);
    deliverFramebufferNotification(0, 0x00000010, nullptr);
    // re-schedule ~60Hz
    sender->setTimeoutMS(16);
}





IOReturn FakeIrisXEFramebuffer::newUserClient(task_t owningTask,
                                              void* securityID,
                                              UInt32 type,
                                              OSDictionary* properties,
                                              IOUserClient **handler)
{
    IOLog("[FakeIrisXEFramebuffer] newUserClient(type=%u)\n", type);

    //
    // Call the REAL IOFramebuffer::newUserClient !!!
    // We do this because WindowServer REQUIRES the real framebuffer UC.
    //

    IOFramebuffer* fb = OSDynamicCast(IOFramebuffer, this);

    if (!fb) {
        IOLog("[FakeIrisXEFramebuffer] ERROR: this is not an IOFramebuffer\n");
        return kIOReturnUnsupported;
    }

    //
    // Real IOFramebuffer::newUserClient has signature:
    // IOReturn newUserClient(task_t, void*, UInt32, IOUserClient**)
    //
    // So we pass ONLY 4 args, not 5.
    //

    IOReturn ret = fb->IOFramebuffer::newUserClient(owningTask,
                                                    securityID,
                                                    type,
                                                    handler);

    if (ret != kIOReturnSuccess) {
        IOLog("[FakeIrisXEFramebuffer] real IOFramebuffer::newUserClient failed (%x)\n", ret);
        return ret;
    }

    IOLog("[FakeIrisXEFramebuffer] returned REAL IOFramebufferUserClient OK\n");
    return kIOReturnSuccess;
}








// Tiger Lake register addresses (verified from your system)
#define TRANS_CONF_A        0x70008
#define TRANS_HTOTAL_A      0x60000
#define TRANS_HBLANK_A      0x60004
#define TRANS_HSYNC_A       0x60008
#define TRANS_VTOTAL_A      0x6000C
#define TRANS_VBLANK_A      0x60010
#define TRANS_VSYNC_A       0x60014
#define TRANS_DDI_FUNC_CTL_A 0x60400
#define TRANS_CLK_SEL_A     0x46140

#define PLANE_CTL_1_A       0x70180
#define PLANE_SURF_1_A      0x7019C
#define PLANE_STRIDE_1_A    0x70188
#define PLANE_POS_1_A       0x7018C
#define PLANE_SIZE_1_A      0x70190

#define LCPLL1_CTL          0x46010  // DPLL0 on Tiger Lake

// DDI registers (not in your dump, using standard addresses)
#define DDI_BUF_CTL_A       0x64000
#define DDI_BUF_TRANS_A     0x64E00



// Try both panel power locations
#define PP_STATUS_OLD       0x61200  // Pre-TGL
#define PP_CONTROL_OLD      0x61204
#define PP_STATUS_NEW       0xC7200  // TGL
#define PP_CONTROL_NEW      0xC7204
#define PP_ON_DELAYS_NEW    0xC7208
#define PP_OFF_DELAYS_NEW   0xC720C

// Backlight
#define BXT_BLC_PWM_CTL1    0xC8250
#define BXT_BLC_PWM_CTL2    0xC8254

#define PLANE_OFFSET_1_A 0x00000000





IOReturn FakeIrisXEFramebuffer::enableController() {
    IOLog("🟢 V38: enableController() — STRIDE FIX (writing BYTES not blocks)\n");
    IOSleep(30);

    if (!mmioBase || !framebufferMemory) {
        IOLog("❌ MMIO or framebuffer not set\n");
        return kIOReturnError;
    }

    // ---- constants (Tiger Lake) ----
    const uint32_t PIPECONF_A       = 0x70008;
    const uint32_t PIPE_SRC_A       = 0x6001C;

    auto rd = [&](uint32_t off) { return safeMMIORead(off); };
    auto wr = [&](uint32_t off, uint32_t val) { safeMMIOWrite(off, val); };

    #define LOG_FLUSH(msg) do { IOLog msg ; IOSleep(30); } while (0)

    const uint32_t width  = H_ACTIVE;   // 1920
    const uint32_t height = V_ACTIVE;   // 1080
    const IOPhysicalAddress phys = framebufferMemory->getPhysicalAddress();

    IOLog("DEBUG[V38]: reading initial state…\n");
    IOLog("  PLANE_CTL_1_A (before):   0x%08X\n", rd(PLANE_CTL_1_A));
    IOLog("  PLANE_SURF_1_A (before):  0x%08X\n", rd(PLANE_SURF_1_A));
    IOLog("  PLANE_STRIDE_1_A (before):0x%08X\n", rd(PLANE_STRIDE_1_A));
    IOLog("  TRANS_CONF_A (before):    0x%08X\n", rd(TRANS_CONF_A));
    IOLog("  PIPECONF_A (before):      0x%08X\n", rd(PIPECONF_A));
    LOG_FLUSH(("DEBUG[V38]: …state read complete.\n"));

    // --- 1) Program visible pipe source ---
    wr(PIPE_SRC_A, ((width - 1) << 16) | (height - 1));
    IOLog("✅ PIPE_SRC_A set to %ux%u (reg=0x%08X)\n", width, height, rd(PIPE_SRC_A));

    // --- 2) Program plane position/size ---
    wr(PLANE_POS_1_A, 0x00000000);
    wr(PLANE_SIZE_1_A, ((height - 1) << 16) | (width - 1));
    IOLog("✅ PLANE_POS_1_A=0x%08X, PLANE_SIZE_1_A=0x%08X\n",
          rd(PLANE_POS_1_A), rd(PLANE_SIZE_1_A));

   
    
    

    
    
    
    // --------- MAP FB INTO GGTT -----------

    if (!mapFramebufferIntoGGTT()) {
         IOLog("❌ GGTT mapping failed\n");
         return kIOReturnError;
     }

     // --------- PROGRAM PLANE SURFACE ---------
     wr(PLANE_SURF_1_A, fbGGTTOffset);
     IOLog("PLANE_SURF_1_A = 0x%08X\n", rd(PLANE_SURF_1_A));
   
    
    

    
    
    
    
    // ddb entry for pipe a plane 1
    const uint32_t PLANE_BUF_CFG_1_A = 0x70140;
    wr(PLANE_BUF_CFG_1_A, (0x07FFu << 16) | 0x000u);
    IOLog("DDB buffer assigned (PLANE_BUF_CFG_1_A=0x%08X)\n", rd(PLANE_BUF_CFG_1_A));

    
    
    // === TIGER LAKE WATERMARK / FIFO FIX (THIS KILLS THE FLICKERING LINES) ===
    wr(0xC4060, 0x00003FFF);   // WM_LINETIME_A  – increase line time watermark
    wr(0xC4064, 0x00000010);   // WM0_PIPE_A     – conservative level 0
    wr(0xC4068, 0x00000020);   // WM1_PIPE_A     – level 1
    wr(0xC406C, 0x00000040);   // WM2_PIPE_A     – level 2
    wr(0xC4070, 0x00000080);   // WM3_PIPE_A     – level 3

    // Force maximum priority for primary plane
    wr(0xC4020, 0x0000000F);   // ARB_CTL – give plane highest priority

    IOLog("Tiger Lake FIFO/watermark fix applied \n");
    
    
    
    // --- Program stride (in 64-byte blocks) ---
    const uint32_t strideBytes  = 7680;
    const uint32_t strideBlocks = strideBytes / 64;  // Each unit = 64 bytes
    wr(PLANE_STRIDE_1_A, strideBlocks);
    IOSleep(1);
    uint32_t readBack = rd(PLANE_STRIDE_1_A);
    IOLog("✅ PLANE_STRIDE_1_A programmed: %u blocks (64B each), readback=0x%08X\n", strideBlocks, readBack);
    IOLog("👉 Effective byte stride = %u bytes\n", readBack * 64);


     
     
    uint32_t planeCtl = rd(PLANE_CTL_1_A);

    // enable plane
    planeCtl |= (1u << 31);

    // pixel format ARGB8888 = 0x06 << 24
    planeCtl &= ~(7u << 24);
    planeCtl |= (6u << 24);

    // disable all tiling modes
    planeCtl &= ~(3u << 10);   // bits 11:10 = 00 = linear

    // disable rotation
    planeCtl &= ~(3u << 14);

    // write back
    wr(PLANE_CTL_1_A, planeCtl);

    IOLog("PLANE_CTL_1_A (linear/ARGB8888) = 0x%08X\n", rd(PLANE_CTL_1_A));

    
    
    
    // Disable cursor plane (CURSOR_CTL_A = 0x70080)
    wr(0x70080, 0x00000000);  // CURSOR_CTL = disabled
    wr(0x70084, 0x00000000);  // CURBASE = null
    wr(0x7008C, 0x00000000);   // CUR_POS_A  = 0,0
    IOLog("Cursor plane disabled (0x%08X)\n", rd(0x70080));
    
    
    
    // --- Program Pipe A timings for 1920x1080@60 ---
    const uint32_t HTOTAL_A = 0x60000;
    const uint32_t HBLANK_A = 0x60004;
    const uint32_t HSYNC_A  = 0x60008;
    const uint32_t VTOTAL_A = 0x6000C;
    const uint32_t VBLANK_A = 0x60010;
    const uint32_t VSYNC_A  = 0x60014;

    const uint32_t h_active   = 1920;
    const uint32_t h_frontpor = 88;
    const uint32_t h_sync     = 44;
    const uint32_t h_backpor  = 148;
    const uint32_t h_total    = h_active + h_frontpor + h_sync + h_backpor;

    const uint32_t v_active   = 1080;
    const uint32_t v_frontpor = 4;
    const uint32_t v_sync     = 5;
    const uint32_t v_backpor  = 36;
    const uint32_t v_total    = v_active + v_frontpor + v_sync + v_backpor;

    auto pack = [](uint32_t hi, uint32_t lo){ return ((hi - 1) << 16) | (lo - 1); };

    wr(HTOTAL_A, pack(h_total,  h_active));
    wr(HBLANK_A, pack(h_total,  h_active));
    wr(HSYNC_A,  pack(h_active + h_frontpor + h_sync, h_active + h_frontpor));

    wr(VTOTAL_A, pack(v_total,  v_active));
    wr(VBLANK_A, pack(v_total,  v_active));
    wr(VSYNC_A,  pack(v_active + v_frontpor + v_sync, v_active + v_frontpor));

    IOLog("✅ Pipe A timings set: %ux%u @60 (HTOTAL=0x%08X VTOTAL=0x%08X)\n",
          h_active, v_active, rd(HTOTAL_A), rd(VTOTAL_A));

    // Disable panel fitter / pipe scaler
    const uint32_t PF_CTL_A      = 0x68080;
    const uint32_t PF_WIN_POS_A  = 0x68070;
    const uint32_t PF_WIN_SZ_A   = 0x68074;
    const uint32_t PS_CTRL_1_A   = 0x68180;
    const uint32_t PS_WIN_POS_1A = 0x68170;
    const uint32_t PS_WIN_SZ_1A  = 0x68174;

    wr(PF_CTL_A,     0x00000000);
    wr(PF_WIN_POS_A, 0x00000000);
    wr(PF_WIN_SZ_A,  ((v_active & 0x1FFF) << 16) | (h_active & 0x1FFF));
    wr(PS_CTRL_1_A,  0x00000000);
    wr(PS_WIN_POS_1A,0x00000000);
    wr(PS_WIN_SZ_1A, ((v_active & 0x1FFF) << 16) | (h_active & 0x1FFF));

    IOLog("✅ Panel fitter / pipe scaler forced OFF\n");

    // --- Enable Pipe A then Transcoder A ---
    uint32_t pipeconf = rd(PIPECONF_A);
    pipeconf |= (1u << 31);  // Enable
    pipeconf |= (1u << 30);  // Progressive
    wr(PIPECONF_A, pipeconf);
    IOSleep(5);
    IOLog("✅ PIPECONF_A now=0x%08X\n", rd(PIPECONF_A));

    uint32_t trans = rd(TRANS_CONF_A);
    trans |= (1u << 31);
    wr(TRANS_CONF_A, trans);
    IOSleep(5);
    IOLog("✅ TRANS_CONF_A now=0x%08X\n", rd(TRANS_CONF_A));

    // --- Enable DDI A buffer ---
    uint32_t ddi = rd(DDI_BUF_CTL_A);
    ddi |= (1u << 31);
    ddi &= ~(7u << 24);
    ddi |= (1u << 24);
    wr(DDI_BUF_CTL_A, ddi);
    IOSleep(5);
    IOLog("✅ DDI_BUF_CTL_A now=0x%08X\n", rd(DDI_BUF_CTL_A));


    // --- Power up eDP Panel ---
    const uint32_t PP_CONTROL = 0x00064004;
    const uint32_t PP_STATUS  = 0x00064024;
    wr(PP_CONTROL, (1 << 31) | (1 << 30));
    IOSleep(50);
    for (int i = 0; i < 100; i++) {
        uint32_t status = rd(PP_STATUS);
        if (status & (1 << 31)) {
            IOLog("✅ eDP panel power sequence complete (PP_STATUS=0x%08X)\n", status);
            break;
        }
        IOSleep(10);
    }

    
    
    // --- Enable backlight ---
    const uint32_t BXT_BLC_PWM_FREQ1 = 0x000C8250;
    const uint32_t BXT_BLC_PWM_DUTY1 = 0x000C8254;
    wr(BXT_BLC_PWM_FREQ1, 0x0000FFFF);
    wr(BXT_BLC_PWM_DUTY1, 0x00007FFF);
    wr(BXT_BLC_PWM_CTL1,  0x80000000);
    IOLog("✅ Backlight PWM enabled (50%% duty)\n");
    IOSleep(10);

    
   
    // --- Test pattern ---
    IOLog("DEBUG: Painting solid red pattern…\n");
    if (auto* fb = (uint32_t*)framebufferMemory->getBytesNoCopy()) {
        const uint32_t pixels = width * height;
        // Fill with solid red
        for (uint32_t i = 0; i < pixels; ++i) {
            fb[i] = 0x00FF0000; // XRGB: Red
        }

        // Add white border for verification
        for (uint32_t x = 0; x < width; x++) {
            fb[x] = 0x00FFFFFF;  // Top
            fb[(height-1) * width + x] = 0x00FFFFFF;  // Bottom
        }
        for (uint32_t y = 0; y < height; y++) {
            fb[y * width] = 0x00FFFFFF;  // Left
            fb[y * width + (width-1)] = 0x00FFFFFF;  // Right
        }
    }

    
    
    
    
    IOLog("DEBUG: Flushing display…\n");
    flushDisplay();

    // --- Final diagnostics ---
    IOLog("🔍 FINAL REGISTER DUMP:\n");
    IOLog("  PIPE_SRC_A          = 0x%08X\n", rd(PIPE_SRC_A));
    IOLog("  PLANE_POS_1_A       = 0x%08X\n", rd(PLANE_POS_1_A));
    IOLog("  PLANE_SIZE_1_A      = 0x%08X\n", rd(PLANE_SIZE_1_A));
    IOLog("  PLANE_SURF_1_A      = 0x%08X\n", rd(PLANE_SURF_1_A));
    IOLog("  PLANE_STRIDE_1_A    = 0x%08X\n", rd(PLANE_STRIDE_1_A));
    IOLog("  PLANE_CTL_1_A       = 0x%08X\n", rd(PLANE_CTL_1_A));
    IOLog("  PIPECONF_A          = 0x%08X\n", rd(PIPECONF_A));
    IOLog("  TRANS_CONF_A        = 0x%08X\n", rd(TRANS_CONF_A));
    IOLog("  DDI_BUF_CTL_A       = 0x%08X\n", rd(DDI_BUF_CTL_A));

    IOLog("✅✅✅ enableController(V38) complete.\n");
    return kIOReturnSuccess;
}




IOReturn FakeIrisXEFramebuffer::setAbltFramebuffer(void *buffer) {
  wsFrontBuffer = buffer;           // WS-owned shadow
  return kIOReturnSuccess;
}

IOReturn FakeIrisXEFramebuffer::flushFramebuffer(void) {
  if (!framebufferMemory) return kIOReturnNotReady;

  volatile uint8_t *dst = (uint8_t*)framebufferMemory->getBytesNoCopy();
  volatile uint8_t *src = (uint8_t*) (wsFrontBuffer ? wsFrontBuffer : framebufferMemory->getBytesNoCopy());

  // 1920x1080 ARGB8888 line copy (tearing-safe: wait vblank before last few lines)
  const uint32_t pitch = 1920 * 4;
  const uint32_t lines = 1080;

  // simple vblank-then-memcpy (you can optimize later)
  waitVBlank();
  bcopy((const void*)src, (void*)dst, pitch * lines);

  // plane surface “touch” is optional since we’re not changing SURF; do a readback fence:
//  (void) safeMMIORead(PLANE_CTL_1_A);

  return kIOReturnSuccess;
}



void FakeIrisXEFramebuffer::waitVBlank() {
    const uint32_t PIPE_DSL_A = 0x60000 + 0x1A0;
    uint32_t last = safeMMIORead(PIPE_DSL_A);
    const int MAX_ITER = 200000;
    for (int i = 0; i < MAX_ITER; ++i) {
        uint32_t now = safeMMIORead(PIPE_DSL_A);
        if (now < last) return; // wrapped -> vblank
        last = now;
        if ((i & 0x3FFF) == 0) IOSleep(1); // every ~16384 iterations yield to scheduler
    }
    IOLog("⚠️ waitVBlank: timeout after %d iterations\n", MAX_ITER);
}






bool FakeIrisXEFramebuffer::mapFramebufferIntoGGTT()
{
    // -----------------------------
    // 1) Read BAR1 = GTTMMADR
    // -----------------------------
    uint64_t bar1Lo = pciDevice->configRead32(0x18) & ~0xF;
    uint64_t bar1Hi = pciDevice->configRead32(0x1C);
    uint64_t gttPhys = (bar1Hi << 32) | bar1Lo;

    IOLog("🟢 BAR1 (GTTMMADR) = 0x%llX\n", gttPhys);

    // Map full 16MB GGTT aperture
    IOMemoryDescriptor* gttDesc =
        IOMemoryDescriptor::withPhysicalAddress(gttPhys, 0x1000000, kIODirectionInOut);

    if (!gttDesc) {
        IOLog("❌ Failed to create GGTT descriptor\n");
        return false;
    }

    IOMemoryMap* gttMap = gttDesc->map();
    if (!gttMap) {
        IOLog("❌ Failed to map GTTMMADR\n");
        gttDesc->release();
        return false;
    }

    gttVa = reinterpret_cast<void*>(gttMap->getVirtualAddress());
    IOLog("🟢 GTTMMADR mapped at VA=%p\n", gttVa);


    if (!gttVa || !framebufferMemory) {
        IOLog("❌ GGTT map: missing gttVa or framebufferMemory\n");
        return false;
    }

    volatile uint64_t* ggtt = reinterpret_cast<volatile uint64_t*>(gttVa);

    const uint32_t kPageSize = 4096;
    const uint64_t kPteFlags = 0x0000000000000003ULL; // Present + writable


    // -----------------------------------------
    // 2) Final, correct GGTT offset for FB
    // -----------------------------------------
    // Your framebuffer will appear at GPU VA = 0x08000000
    // SAFE region: 128MB–144MB is unused by GuC / engines
    fbGGTTOffset = 0x00000800;

    uint32_t ggttBaseIndex = fbGGTTOffset >> 12;

    IOLog("🟢 GGTT mapping: fbGGTTOffset=0x%08X index=%u\n",
          fbGGTTOffset, ggttBaseIndex);


    // -----------------------
    // 3) Walk physical pages
    // -----------------------
    IOByteCount fbSize = framebufferMemory->getLength();
    IOByteCount offset = 0;

    uint32_t page = 0;

    while (offset < fbSize)
    {
        IOByteCount segLen = 0;
        IOPhysicalAddress segPhys =
            framebufferMemory->getPhysicalSegment(offset, &segLen);

        if (!segPhys || segLen == 0) {
            IOLog("❌ GGTT map: getPhysicalSegment failed at offset 0x%llX\n",
                  (uint64_t)offset);
            return false;
        }

        // Page-align
        segLen &= ~(kPageSize - 1);
        if (segLen == 0) {
            IOLog("❌ GGTT map: segment < 4KB\n");
            return false;
        }

        IOLog("   ➤ Physical segment: phys=0x%llX len=0x%llX\n",
              (uint64_t)segPhys, (uint64_t)segLen);


        for (IOByteCount segOff = 0;
             segOff < segLen && offset < fbSize;
             segOff += kPageSize, offset += kPageSize, ++page)
        {
            uint64_t phys = (uint64_t)(segPhys + segOff);

            uint32_t ggttIndex = ggttBaseIndex + page;

            uint64_t pte = (phys & ~0xFFFULL) | kPteFlags;
            ggtt[ggttIndex] = pte;

            uint64_t verify = ggtt[ggttIndex];

            IOLog("   GGTT[%u] = 0x%016llX (verify=0x%016llX)\n",
                  ggttIndex, pte, verify);
        }
    }

    IOLog("🟢 GGTT mapping complete (%u pages)\n", page);

    return true;
}





 
 
void FakeIrisXEFramebuffer::disableController() {
    if(!mmioBase) return;
    
    // Disable pipe (example offsets)
    const uint32_t DSPACNTR = 0x70180;
    *(volatile uint32_t*)(mmioBase + DSPACNTR) &= ~0x80000000;
    IOLog("✅ Controller disabled\n");
}
 
 
 
 


IOReturn FakeIrisXEFramebuffer::getOnlineState(IOIndex connectIndex, bool* online)
{
    if (connectIndex != 0) return kIOReturnBadArgument;
    *online = displayOnline;
    IOLog("getOnlineState: %d\n", displayOnline);
    return kIOReturnSuccess;
}


IOReturn FakeIrisXEFramebuffer::setAttributeForConnection(IOIndex, IOSelect, uintptr_t) {
    return kIOReturnSuccess;
}




IOReturn FakeIrisXEFramebuffer::setOnlineState(IOIndex connectIndex, bool online)
{
    if (connectIndex != 0) return kIOReturnBadArgument;
       IOLog("setOnlineState: %d\n", online);
       displayOnline = online; // Update member variable
       return kIOReturnSuccess;
   }


bool FakeIrisXEFramebuffer::setupDisplayModes() {
    IOLog("setupDisplayModes() called\n");
    
       setNumberOfDisplays(1);
       return true;
}






bool FakeIrisXEFramebuffer::isConsoleDevice() const {
    return true;
}



bool FakeIrisXEFramebuffer::getIsUsable() const {
    return true;
}

IOReturn FakeIrisXEFramebuffer::setOnline(bool online) {

    IOLog("setOnline() called\n");
        displayOnline = online;
        // Return kIOReturnSuccess for both online/offline states if you handle them.
        return kIOReturnSuccess;
}

IOReturn FakeIrisXEFramebuffer::getConnectionFlags(IOIndex connectIndex, UInt32* flags) {
    if (!flags) return kIOReturnBadArgument;
    *flags = kIOConnectionBuiltIn;  // Essential
    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::notifyServer(IOSelect message, void* data, size_t dataSize) {
    IOLog("notifyServer() called: message = 0x%08X\n", message);

    switch (message) {
        case kIOFBNotifyDisplayModeChange:
            // TODO: implement display mode change handling
            return kIOReturnSuccess;

        default:
            IOLog("❓ notifyServer(): unhandled message = 0x%08X\n", message);
            return kIOReturnUnsupported;
    }
}




IOReturn FakeIrisXEFramebuffer::getTimingInfoForDisplayMode(
    IODisplayModeID displayMode,
    IOTimingInformation* infoOut)
{
    if (!infoOut || displayMode != 0) {
        return kIOReturnUnsupportedMode;
    }
    
    bzero(infoOut, sizeof(IOTimingInformation));
    
    // Proper 1920x1080@60Hz timing
    infoOut->appleTimingID = kIOTimingIDDefault;
    infoOut->flags = kIOTimingInfoValid_AppleTimingID;
    infoOut->detailedInfo.v1.horizontalActive = 1920;
    infoOut->detailedInfo.v1.verticalActive = 1080;
    infoOut->detailedInfo.v1.pixelClock = 148500000; // 148.5 MHz for 1080p60
    
    
    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::setCLUTWithEntries(IOColorEntry* colors,
                                                   UInt32 firstIndex,
                                                   UInt32 numEntries,
                                                   IOOptionBits options) {
    if (!colors || numEntries == 0 || firstIndex + numEntries > 256) {
        return kIOReturnBadArgument;
    }
    
    // Store color entries for software lookup table
    if (!clutTable) {
        clutTable = (IOColorEntry*)IOMalloc(256 * sizeof(IOColorEntry));
        if (!clutTable) return kIOReturnNoMemory;
    }
    
    for (UInt32 i = 0; i < numEntries; i++) {
        clutTable[firstIndex + i] = colors[i];
    }
    
    IOLog("✅ CLUT updated: %d entries starting at %d\n", numEntries, firstIndex);
    return kIOReturnSuccess;
}


    

IOReturn FakeIrisXEFramebuffer::setGammaTable(UInt32 ch, UInt32 count, UInt32 width, void *data) {
  if (ch == 3 && width == 16 && count <= 256 && data) {
    gamma.set = true;
    bcopy(data, gamma.table, count*ch*sizeof(UInt16));
    // Optional: program PIPE gamma LUT later; for now, accept.
    return kIOReturnSuccess;
  }
  return kIOReturnUnsupported;
}






IOReturn FakeIrisXEFramebuffer::getGammaTable(UInt32 channelCount,
                                              UInt32* dataCount,
                                              UInt32* dataWidth,
                                              void** data) {
    if (!dataCount || !dataWidth || !data) {
        return kIOReturnBadArgument;
    }
    
    if (!gammaTable) {
        return kIOReturnNotFound;
    }
    
    *dataCount = 256;  // Standard 256-entry gamma table
    *dataWidth = 8;    // 8-bit per channel
    *data = gammaTable;
    
    return kIOReturnSuccess;
}





IOReturn FakeIrisXEFramebuffer::getAttributeForConnection(IOIndex connectIndex,
                                                         IOSelect attribute,
                                                         uintptr_t* value)
{
    IOLog("getAttributeForConnection(%d, 0x%x)\n", connectIndex, attribute);
    
    if (connectIndex != 0) return kIOReturnBadArgument; // Only handle connection 0

        switch (attribute) {
            case kConnectionSupportsAppleSense:
            case kConnectionSupportsLLDDCSense:
            case kConnectionSupportsHLDDCSense:
            case kConnectionSupportsDDCSense:
            case kIOCapturedAttribute:
                *value = 1;
                return kIOReturnSuccess;

            case kConnectionIsOnline:
                *value=1;
                return kIOReturnSuccess;

            case kIOSystemPowerAttribute:
                *value=1;
                return kIOReturnSuccess;
                
            case kIOHardwareCursorAttribute:
                *value = 0; // Report hardware cursor support (even if fake)
                return kIOReturnSuccess;

            case kConnectionFlags:
                *value = kIOConnectionBuiltIn; // Only report built-in for simplicity
                return kIOReturnSuccess;

            default:
                return kIOReturnUnsupported;
        }
    }






const char* FakeIrisXEFramebuffer::getPixelFormats(void) {
    return "ARGB8888";
}





    
IOReturn FakeIrisXEFramebuffer::setCursorImage(void* cursorImage) {
    if (!cursorImage || !cursorMemory) {
        return kIOReturnBadArgument;
    }
    
    void* cursorBuffer = cursorMemory->getBytesNoCopy();
    if (!cursorBuffer) {
        return kIOReturnError;
    }
    
    // Copy cursor data (assuming 32x32 ARGB cursor)
    bcopy(cursorImage, cursorBuffer, 32 * 32 * 4);
    
    IOLog("✅ Cursor image updated\n");
    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::setCursorState(SInt32 x, SInt32 y, bool visible) {
    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::registerForInterruptType(IOSelect interruptType,
                                                         IOFBInterruptProc proc,
                                                         void* ref,
                                                         void** interruptRef) {
    if (!proc || !interruptRef) {
        return kIOReturnBadArgument;
    }
    
    InterruptInfo* info = (InterruptInfo*)IOMalloc(sizeof(InterruptInfo));
    if (!info) return kIOReturnNoMemory;
    
    info->type = interruptType;
    info->proc = proc;
    info->ref = ref;
    
    // Add to interrupt list
    if (!interruptList) {
        interruptList = OSArray::withCapacity(4);
    }
    
    OSData* infoData = OSData::withBytes(info, sizeof(InterruptInfo));
    if (infoData) {
        interruptList->setObject(infoData);
        infoData->release();
    }
    
    *interruptRef = info;
    
    IOLog("✅ Interrupt registered for type 0x%x\n", interruptType);
    return kIOReturnSuccess;
}




IOReturn FakeIrisXEFramebuffer::unregisterInterrupt(void* interruptRef) {
    if (!interruptRef || !interruptList) {
        return kIOReturnBadArgument;
    }
    
    // Find and remove from interrupt list
    for (unsigned int i = 0; i < interruptList->getCount(); i++) {
        OSData* data = OSDynamicCast(OSData, interruptList->getObject(i));
        if (data && data->getBytesNoCopy() == interruptRef) {
            interruptList->removeObject(i);
            IOFree(interruptRef, sizeof(InterruptInfo));
            IOLog("✅ Interrupt unregistered\n");
            return kIOReturnSuccess;
        }
    }
    
    return kIOReturnNotFound;
}





IOReturn FakeIrisXEFramebuffer::createAccelTask(mach_port_t* port) {

    return kIOReturnUnsupported;
}





void FakeIrisXEFramebuffer::publishDisplay() {
    
    if (!fullyInitialized) {
         IOLog("publishDisplay called before full initialization\n");
         return;
     }
    
    IOLog("FakeIrisXEFramebuffer::publishDisplay() called\n");

    if(displayPublished) return; // ✅ Prevent duplicates
        displayPublished = true;
    
    IOService* display = new IOService;
    if (display && display->init()) {
        display->setName("display0");
        display->setProperty("DisplayProductID", (uint32_t)0x717);
        display->setProperty("DisplayVendorID", (uint32_t)0x756e6b6e); // 'unkn'
        display->setProperty("IODisplayPrefsKey", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/GFX0@2/display0");
        display->setProperty("IOFBIsMainDisplay", kOSBooleanTrue);
        display->setProperty("IODisplayIsActive", kOSBooleanTrue);
        display->attach(this);
        display->registerService();
        display->release(); // ✅ prevent leak
    }
    else {
            IOLog("❌ publishDisplay(): Failed to create or initialize childDisplay\n");
            if (display) display->release();
            return; // Exit if childDisplay failed
        }
    
    
    // CRITICAL FIX: Correctly build the IOFramebufferDisplays dictionary structure
      OSDictionary* singleDisplayInfo = OSDictionary::withCapacity(1);
      if (!singleDisplayInfo) {
          IOLog("❌ publishDisplay(): Failed to create singleDisplayInfo dictionary\n");
          return;
      }
      singleDisplayInfo->setObject("IODisplayPrefsKey", OSSymbol::withCString("IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/GFX0@2/display0"));

      OSArray* displayList = OSArray::withCapacity(1);
      if (!displayList) {
          IOLog("❌ publishDisplay(): Failed to create displayList array\n");
          singleDisplayInfo->release();
          return;
      }
      displayList->setObject(singleDisplayInfo); // Add the display info dictionary to the array

      OSDictionary* fbDisplays = OSDictionary::withCapacity(1);
      if (!fbDisplays) {
          IOLog("❌ publishDisplay(): Failed to create fbDisplays dictionary\n");
          singleDisplayInfo->release();
          displayList->release();
          return;
      }
      fbDisplays->setObject("display0", displayList); // Add the array to the main dictionary

      setProperty("IOFramebufferDisplays", fbDisplays);

      IOLog("✅ publishDisplay(): display0 and IOFramebufferDisplays now published\n");

      // Release local references (IOKit retains them)
      singleDisplayInfo->release();
      displayList->release();
      fbDisplays->release();
    
    
    
    deliverFramebufferNotification(0, kIOFBNotifyDisplayAdded, nullptr);
        deliverFramebufferNotification(0, kIOFBNotifyDisplayModeChange, nullptr);
        IOLog("✅ publishDisplay(): Sent immediate notifications\n");
    
          }




void FakeIrisXEFramebuffer::vsyncTimerFired(OSObject* owner, IOTimerEventSource* sender)
{
    auto fb = OSDynamicCast(FakeIrisXEFramebuffer, owner);
    if (!fb) return;

    IOLockLock(fb->timerLock);

    if (!fb->driverActive || fb->isInactive() || !fb->fullyInitialized) {
        IOLockUnlock(fb->timerLock);
        return;
    }

    // Optional: log trace or mark vsync time
    IOLog("🔁 vsyncTimerFired\n");

    // Notify vsync interrupt source
    if (fb->vsyncSource) {
        fb->vsyncSource->interruptOccurred(nullptr, nullptr, 0);
    }

    fb->deliverFramebufferNotification(0, kIOFBVsyncNotification, nullptr);

    // Reschedule
    if (fb->vsyncTimer && fb->driverActive) {
        fb->vsyncTimer->setTimeoutMS(16); // simulate ~60Hz
    }

    IOLockUnlock(fb->timerLock);
}



bool FakeIrisXEFramebuffer::setupWorkLoop() {
    workLoop = IOWorkLoop::workLoop();
    if (!workLoop) {
        IOLog("Failed to create work loop\n");
        return false;
    }

    commandGate = IOCommandGate::commandGate(this);
    if (!commandGate) {
        IOLog("Failed to create command gate\n");
        workLoop->release();
        workLoop = nullptr;
        return false;
    }
    
    // FIX: Check if addEventSource succeeds
    if (workLoop->addEventSource(commandGate) != kIOReturnSuccess) {
        IOLog("Failed to add command gate to work loop\n");
        commandGate->release();
        commandGate = nullptr;
        workLoop->release();
        workLoop = nullptr;
        return false;
    }
    
    return true;
}



    
void FakeIrisXEFramebuffer::vsyncOccurred(OSObject* owner, IOInterruptEventSource* src, int count)
{
    auto fb = OSDynamicCast(FakeIrisXEFramebuffer, owner);
        if (!fb) return;
        // Notify WindowServer
        fb->deliverFramebufferNotification(0, kIOFBVsyncNotification, nullptr);
    }





IOReturn FakeIrisXEFramebuffer::setDisplayMode(IODisplayModeID mode, IOIndex depth) {
    
    if (mode != 0) return kIOReturnUnsupportedMode;  // Allow mode 0
        if (depth != 0 && depth != 32) return kIOReturnUnsupportedMode;  // Allow depth 0 or 32
        
        currentMode = mode;
        currentDepth = depth;
        
        return kIOReturnSuccess;
    }



IODeviceMemory* FakeIrisXEFramebuffer::getVRAMRange() {
    if (!framebufferMemory) return nullptr;
    
    IOPhysicalAddress phys = framebufferMemory->getPhysicalAddress();
    IOByteCount length = framebufferMemory->getLength();
    
    // MUST return a new IODeviceMemory object each time
    return IODeviceMemory::withRange(phys, length);
}



IOReturn FakeIrisXEFramebuffer::createSharedCursor(IOIndex index, int version) {
    if (index != 0 || version != 2) {
        return kIOReturnBadArgument;
    }
    
    if (!cursorMemory) {
        cursorMemory = IOBufferMemoryDescriptor::withOptions(
            kIOMemoryKernelUserShared | kIODirectionInOut,
            4096,  // 4KB for cursor
            page_size
        );
        
        if (!cursorMemory) {
            return kIOReturnNoMemory;
        }
        
        bzero(cursorMemory->getBytesNoCopy(), 4096);
    }
    
    IOLog("✅ Shared cursor created\n");
    return kIOReturnSuccess;
}




IOReturn FakeIrisXEFramebuffer::setBounds(IOIndex index, IOGBounds *bounds) {
    IOLog("setBounds() called\n");
    if (bounds) {
        bounds->minx = 0;
        bounds->miny = 0;
        bounds->maxx = 1920;
        bounds->maxy = 1080;
    }
    return kIOReturnSuccess;
}




IOReturn FakeIrisXEFramebuffer::clientMemoryForType(UInt32 type, UInt32* flags, IOMemoryDescriptor** memory)
{
    IOLog("FakeIrisXEFramebuffer::clientMemoryForType() - type: %u\n", type);

    // Cursor memory (keep existing behavior)
    if (type == kIOFBCursorMemory && cursorMemory) {
        cursorMemory->retain();       // caller expects a retained descriptor
        *memory = cursorMemory;
        if (flags) *flags = 0;
        return kIOReturnSuccess;
    }

    // ---- IMPORTANT: return IODeviceMemory for system aperture ----
    if (type == kIOFBSystemAperture) {
        IODeviceMemory *devMem = getVRAMRange(); // your helper that returns IODeviceMemory::withRange(...)
        if (!devMem) {
            IOLog("clientMemoryForType: failed to get VRAM IODeviceMemory\n");
            return kIOReturnNoMemory;
        }

        // getVRAMRange returns a *new* IODeviceMemory object (with refcount 1),
        // so hand it to the caller directly.
        *memory = devMem;
        if (flags) *flags = 0; // allow read / write
        IOLog("clientMemoryForType: returned IODeviceMemory for kIOFBSystemAperture phys=0x%llx len=0x%llx\n",
              (unsigned long long)devMem->getPhysicalAddress(), (unsigned long long)devMem->getLength());
        return kIOReturnSuccess;
    }

    // VRAM type: provide IODeviceMemory (if you also support this)
    if (type == kIOFBVRAMMemory && framebufferSurface) {
        framebufferSurface->retain();
        *memory = framebufferSurface;
        if (flags) *flags = 0;
        return kIOReturnSuccess;
    }

    return kIOReturnUnsupported;
}




// ==== REAL FLUSH WORK (runs on workloop thread) ====
IOReturn FakeIrisXEFramebuffer::performFlushNow()
{
    IOLog("FakeIrisXEFB::performFlushNow(): running\n");

    if (!framebufferMemory)
        return kIOReturnNotReady;

    void *dst = framebufferMemory->getBytesNoCopy();
    if (!dst)
        return kIOReturnError;

    uint32_t *p = (uint32_t *)dst;
    uint32_t pixels = 1920 * 1080;

    OSMemoryBarrier();
    (void) safeMMIORead(PLANE_CTL_1_A);

    IOLog("FakeIrisXEFB::performFlushNow(): done\n");
    return kIOReturnSuccess;
}



// ==== commandGate wrapper ====
IOReturn FakeIrisXEFramebuffer::staticPerformFlush(
    OSObject *owner,
    void *arg0, void *arg1,
    void *arg2, void *arg3)
{
    FakeIrisXEFramebuffer *fb =
        OSDynamicCast(FakeIrisXEFramebuffer, owner);

    if (!fb) return kIOReturnBadArgument;
    return fb->performFlushNow();
}



// ==== PUBLIC API THAT WINDOWSERVER CALLS ====
IOReturn FakeIrisXEFramebuffer::flushDisplay(void)
{
    IOLog("FakeIrisXEFB::flushDisplay(): schedule work\n");

    if (!commandGate || !workLoop)
        return performFlushNow(); // fallback safe

    IOReturn r = commandGate->runAction(
        &FakeIrisXEFramebuffer::staticPerformFlush
    );

    IOLog("FakeIrisXEFB::flushDisplay(): runAction returned %x\n", r);
    return r;
}






void FakeIrisXEFramebuffer::deliverFramebufferNotification(IOIndex index, UInt32 event, void* info) {
    IOLog("📩 deliverFramebufferNotification() index=%u event=0x%08X\n", index, event);
    
    // Create proper notification info structure if needed
    switch (event) {
        case kIOFBNotifyDisplayModeChange:
        case kIOFBNotifyDisplayAdded:
        case kIOFBConfigChanged:
        case kIOFBVsyncNotification:
            super::deliverFramebufferNotification(index, (void*)(uintptr_t)event);
            break;
        default:
            super::deliverFramebufferNotification(index, info);
            break;
    }
}
    





IOReturn FakeIrisXEFramebuffer::setNumberOfDisplays(UInt32 count)
{
    
    
    IOLog("🖥️ setNumberOfDisplays(%u)\n", count);
    return kIOReturnSuccess;
}






IOReturn FakeIrisXEFramebuffer::setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice) {
    IOLog("💡 setPowerState() called: %lu. (V26: Doing nothing.)\n", (unsigned long)powerStateOrdinal);
    
    // We do *not* want to do *anything* here.
    // Power is handled by our timer.
    
    return IOPMAckImplied;
}






IOItemCount FakeIrisXEFramebuffer::getDisplayModeCount(void)
{
    return 1; // Must return at least 1 mode
}


IOReturn FakeIrisXEFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes) {
    if (allDisplayModes)
        allDisplayModes[0] = 0;
    
    if (!allDisplayModes) {
        IOLog("⚠️ getDisplayModes(): null pointer\n");
        return kIOReturnSuccess;
    }

    
    return kIOReturnSuccess;
}



UInt64 FakeIrisXEFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
    IOLog("🧠 getPixelFormatsForDisplayMode(): mode=%u depth=%u\n", displayMode, depth);

    if (displayMode == 0 && depth == 32) {
        return (1ULL << 0); // Index 0 = ARGB8888
    }

    return 1; // supported
}





IOReturn FakeIrisXEFramebuffer::getPixelInformation(
    IODisplayModeID displayMode,
    IOIndex depth,
    IOPixelAperture aperture,
    IOPixelInformation* info)
{
    if (!info) return kIOReturnBadArgument;
    if (displayMode != 0 || depth != 32) return kIOReturnUnsupportedMode;

    bzero(info, sizeof(IOPixelInformation));

    // Matches PLANE_CTL format field = 0x0 (ARGB8888)
    strlcpy(info->pixelFormat, "ARGB8888", sizeof(info->pixelFormat));

    info->pixelType        = kIO32ARGBPixelFormat; // Apple expects ARGB even for ARGB8888 layouts
    info->componentCount   = 4;                    // X (ignored) + R + G + B
    info->bitsPerComponent = 8;
    info->bitsPerPixel     = 32;
    info->bytesPerRow      = 1920 * 4;
    info->activeWidth      = 1920;
    info->activeHeight     = 1080;

    // Color masks for ARGB8888 layout: [31:24]=X, [23:16]=R, [15:8]=G, [7:0]=B
    info->componentMasks[0] = 0x00FF0000; // Red
    info->componentMasks[1] = 0x0000FF00; // Green
    info->componentMasks[2] = 0x000000FF; // Blue
    info->componentMasks[3] = 0xFF000000;

    flushDisplay();

    return kIOReturnSuccess;
}







IOIndex FakeIrisXEFramebuffer::getAperture() const {
    return kIOFBSystemAperture;
}




IODeviceMemory* FakeIrisXEFramebuffer::getApertureRange(IOPixelAperture aperture) {
    IOLog("✅ getApertureRange() called with aperture = %d\n", aperture);

    if (aperture != kIOFBSystemAperture) {
        IOLog("❌ Unsupported aperture requested\n");
        return nullptr;
    }

    return getVRAMRange(); // ✅ Clean and works
}






IOReturn FakeIrisXEFramebuffer::getFramebufferOffsetForX_Y(IOPixelAperture aperture, SInt32 x, SInt32 y, UInt32 *offset)
{
    if (!offset || aperture != kIOFBSystemAperture)
        return kIOReturnBadArgument;

    // Linear framebuffer, so no X/Y offset — return 0
    *offset = 0;
    return kIOReturnSuccess;
}


IOReturn FakeIrisXEFramebuffer::getCurrentDisplayMode(IODisplayModeID* mode, IOIndex* depth) {
    if (mode) *mode = currentMode;
    if (depth) *depth = currentDepth;
    return kIOReturnSuccess;
}





IOReturn FakeIrisXEFramebuffer::setMode(IODisplayModeID displayMode, IOOptionBits options, UInt32 depth) {
    IOLog("FakeIrisXEFramebuffer::setMode() CALLED! Mode ID: %u, Options: 0x%x, Depth: %u\n", displayMode, options, depth);


    currentMode = displayMode;
    currentDepth = depth;

    if (depth == 32) {
        setProperty("IOFBCurrentPixelFormat", OSString::withCString("ARGB8888"));
    }

    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::getInformationForDisplayMode(
    IODisplayModeID mode,
    IODisplayModeInformation* info)
{
    IOLog("🧪 getInformationForDisplayMode() CALLED for mode = %d\n", mode);

    if (!info || mode != 0) {
        IOLog("🛑 Invalid info pointer or mode not supported\n");
        return kIOReturnUnsupportedMode;
    }

    bzero(info, sizeof(IODisplayModeInformation));
    info->maxDepthIndex = 0;
    info->nominalWidth = 1920;
    info->nominalHeight = 1080;
    info->refreshRate = (60 << 16); // 60Hz in fixed-point
   
    // Set timing information
        info->reserved[0] = kIOTimingIDDefault;
        info->reserved[1] = 0;


    IOLog("✅ Returning display mode info: 1920x1080 @ 60Hz\n");

    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::getStartupDisplayMode(IODisplayModeID *modeID, IOIndex *depth)
{
    IOLog("getStartupDisplayMode() called\n");
    if (modeID) *modeID = 0; // Not 1!
    if (depth)  *depth  = 0;
    return kIOReturnSuccess;
}



UInt32 FakeIrisXEFramebuffer::getConnectionCount() {
    IOLog("getConnectionCount() called\n");
    return 1; // 1 display connection
}





IOReturn FakeIrisXEFramebuffer::setAttribute(IOSelect attribute, uintptr_t value) {
    IOLog("setAttribute(0x%x, 0x%lx)\n", attribute, value);

    if (!fullyInitialized || !driverActive) {
        return kIOReturnNotReady;
    }

    switch (attribute) {
        case kIOCaptureAttribute:
            IOLog("✅ WindowServer requested capture - flushing\n");
            flushDisplay(); // 👈 critical
            return kIOReturnSuccess;

        case kIOPowerAttribute:
            if (value == kIOPMPowerOn) {
                    // keep your pipe on; you can re-enable backlight if you’d turned it off
                  } else {
                    // optionally dim/backlight off
                  }
               
        case kIOSystemPowerAttribute:
            return kIOReturnSuccess;

        default:
            return IOFramebuffer::setAttribute(attribute, value);
    }
}






IOReturn FakeIrisXEFramebuffer::getAttribute(IOSelect attr, uintptr_t *value) {
  switch (attr) {
    case kIOHardwareCursorAttribute: if (value) *value = 0; return kIOReturnSuccess;
    case kIOVRAMSaveAttribute:       if (value) *value = 0; return kIOReturnSuccess;
      case kIOPowerAttribute:          if (value) *value = kIOPMPowerOn; return kIOReturnSuccess;
  }
  return super::getAttribute(attr, value);
}

IOReturn FakeIrisXEFramebuffer::getAttributeForIndex(IOSelect attribute, UInt32 index, UInt32* value) {
    IOLog("🔍 getAttributeForIndex(%u, %u)\n", attribute, index);

    if (!value) return kIOReturnBadArgument;

    switch (attribute) {
        case kIOPowerAttribute:
        case kIOSystemPowerAttribute:
            *value = 1; // Always "on"
            return kIOReturnSuccess;

        default:
            *value = 0;
            return kIOReturnUnsupported;
    }
}



IOReturn FakeIrisXEFramebuffer::setProperties(OSObject* properties) {
    IOLog("🛠 setProperties()\n");
    return kIOReturnSuccess;
}

IOReturn FakeIrisXEFramebuffer::validateDetailedTiming(void* desc, UInt32* score) {
    IOLog("📐 validateDetailedTiming()\n");
    if (score) *score = 0;
    return kIOReturnSuccess;
}

IOReturn FakeIrisXEFramebuffer::setDetailedTimings(OSObject* params) {
    IOLog("📐 setDetailedTimings()\n");
    return kIOReturnUnsupported;
}

IOReturn FakeIrisXEFramebuffer::setInterruptState(void* ref, UInt32 state) {
    IOLog("⚡️ setInterruptState(state=%u)\n", state);
    return kIOReturnSuccess;
}

IOReturn FakeIrisXEFramebuffer::handleEvent(IOFramebuffer* fb, void* ref, UInt32 event, void* info) {
    IOLog("📩 handleEvent(event=%u)\n", event);
    return kIOReturnUnsupported;
}

IOReturn FakeIrisXEFramebuffer::doControl(UInt32 command, void* params, UInt32 size) {
    IOLog("🛠 doControl(cmd=%u, size=%u)\n", command, size);
    return kIOReturnUnsupported;
}

IOReturn FakeIrisXEFramebuffer::extControl(OSObject* params) {
    IOLog("🛠 extControl()\n");
    return kIOReturnUnsupported;
}

void FakeIrisXEFramebuffer::transformLocation(IOGPoint* loc, IOOptionBits options) {
    IOLog("🧭 transformLocation(%d, %d)\n", loc->x, loc->y);
    // No transformation needed
}







#include <libkern/libkern.h> // For kern_return_t, kmod_info_t

// Kext entry/exit points
extern "C" kern_return_t FakeIrisXEFramebuffer_start(kmod_info_t *ki, void *data) {
    return KERN_SUCCESS;
}

extern "C" kern_return_t FakeIrisXEFramebuffer_stop(kmod_info_t *ki, void *data) {
    return KERN_SUCCESS;
}
