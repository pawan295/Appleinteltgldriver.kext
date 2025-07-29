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


using namespace libkern;

// New: Render and Media domain FORCEWAKE_ACK registers for Gen11+
#define FORCEWAKE_ACK_RENDER 0x0A188  // Read-only
#define FORCEWAKE_ACK_MEDIA  0x0A18C  // Optional, already used
#define FORCEWAKE_ACK 0x0A188  // This was probably used as generic

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


#define kIO32BGRAPixelFormat 'BGRA'
#define kIOPixelFormatWideGamut 'wgam'
#define kIOCaptureAttribute 'capt'

#define kIOFBNotifyDisplayAdded  0x00000010
#define kIOFBConfigChanged       0x00000020



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

#define super IOFramebuffer
#define kIOMessageServiceIsRunning 0x00001001



OSDefineMetaClassAndStructors(FakeIrisXEFramebuffer, IOFramebuffer)



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
        if (score) {
            *score += 50000; // Force to beat IONDRVFramebuffer
        }
        // Call super::probe if you want parent class probing, but ensure it doesn't override your score
         IOService* result = super::probe(provider, score);
        if (result) return result;
        return OSDynamicCast(IOService, this); // Return this instance if it matches
    }

    return nullptr; // No match
}


bool FakeIrisXEFramebuffer::init(OSDictionary* dict) {
    IOLog("FakeIrisXEFramebuffer::init() - Entered\n");

    if (!super::init(dict))
        return false;
    
    vramMemory = nullptr;
    pciDevice = nullptr;
    mmioBase = nullptr;
    currentMode = 0;
    currentDepth = 0;
    vramSize = 1920 * 1080 * 4; // 32bpp
    
    return true;
}

/*
// Define power states array — example values
IOPMPowerState FakeIrisXEFramebuffer::powerStates[2] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},              // Off state
    {1, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}  // On state
};

*/


//start
bool FakeIrisXEFramebuffer::start(IOService* provider) {
    IOLog("FakeIrisXEFramebuffer::start() - Entered\n");

    if (!super::start(provider)) {
        IOLog("FakeIrisXEFramebuffer::start() - super::start() failed\n");
        return false;
    }
    
 /*
    // Initialize power management
    PMinit();
    provider->joinPMtree(this);

 
    registerPowerDriver(this, powerStates, 2);
    changePowerStateTo(kPowerStateOn); // Let the system call setPowerState
  
  makeUsable();
*/

    
      pciDevice = OSDynamicCast(IOPCIDevice, provider);
      if (!pciDevice) {
          IOLog("Failed to cast provider to IOPCIDevice\n");
          return false;
      }
      
      // Enable device memory and bus mastering
      pciDevice->setMemoryEnable(true);
      pciDevice->setBusMasterEnable(true);
      
      if (!initializeHardware())
          return false;
      
      if (!setupVRAM())
          return false;
      
      if (!setupDisplayModes())
          return false;

    
    pciDevice->retain();

    
    
    uint32_t pciCommand = pciDevice->configRead16(kIOPCIConfigCommand);
    IOLog("FakeIrisXEFramebuffer::start() - PCI COMMAND = 0x%04X\n", pciCommand);

    uint8_t pciPower = pciDevice->configRead8(0xD4);
    IOLog("FakeIrisXEFramebuffer::start() - PCI Power Control = 0x%02X\n", pciPower);
    
    
    
    
    //ACPI plane walk
    IORegistryEntry *acpiWalker = pciDevice;
    IOLog("🧭 ACPI plane walk from PCI device:\n");

    while ((acpiWalker = acpiWalker->getParentEntry(gIOACPIPlane)) != nullptr) {
        const char* name = acpiWalker->getName();
        const char* location = acpiWalker->getLocation();
        IOLog(" → ACPI node: %s @ %s\n", name ? name : "?", location ? location : "?");

        OSData* adr = OSDynamicCast(OSData, acpiWalker->getProperty("_ADR"));
        if (adr && adr->getLength() == 4) {
            uint32_t adrVal = *(uint32_t*)adr->getBytesNoCopy();
            IOLog("   |_ _ADR = 0x%08X\n", adrVal);
            if (adrVal == 0x00020000) {
                IOLog("✅ Matched _ADR 0x00020000 — this is likely GFX0\n");
            }
        }
    }
    
    // call _DSM
    IOACPIPlatformDevice *acpiDev = nullptr;

    
    //dsm
    IORegistryEntry *parent = pciDevice;
    while ((parent = parent->getParentEntry(gIOACPIPlane)) != nullptr) {
        const char* name = parent->getName();
        const char* location = parent->getLocation();
        IOLog("ACPI Walk: node = %s, location = %s\n", name, location ? location : "null");

        OSData* adr = OSDynamicCast(OSData, parent->getProperty("_ADR"));
        if (adr && adr->getLength() == 4) {
            uint32_t adrVal = *(uint32_t*)adr->getBytesNoCopy();
            IOLog("Found _ADR = 0x%08X\n", adrVal);
            if (adrVal == 0x00020000) {
                acpiDev = OSDynamicCast(IOACPIPlatformDevice, parent);
                break;
            }
        }
    }

    
    // --- Fallback: Manually locate GFX0 via fromPath ---
    if (!acpiDev) {
           IOLog("🔍 Trying fallback path to locate GFX0 via IOACPIPlane\n");
           IORegistryEntry *gfx0Path = IORegistryEntry::fromPath("/_SB/PC00/GFX0", gIOACPIPlane);
           if (gfx0Path) {
               IOLog("✅ Found GFX0 via fromPath fallback\n");
               acpiDev = OSDynamicCast(IOACPIPlatformDevice, gfx0Path);
               if (acpiDev) {
                   IOLog("✅ Found GFX0 as IOACPIPlatformDevice via fallback\n");
               } else {
                   IOLog("❌ Fallback: GFX0 is not IOACPIPlatformDevice\n");
               }
           } else {
               IOLog("❌ Fallback: GFX0 path not found in IOACPIPlane\n");
           }
       }

    if (acpiDev) {
        IOLog("✅ FakeIrisXEFramebuffer::start() - Found ACPI parent: %s\n", acpiDev->getName());

        
        // _DSM eval block
        OSObject *params[4];
        uint8_t uuid[16] = { 0xA0, 0x12, 0x93, 0x6E, 0x50, 0x9A, 0x4C, 0x5B,
                             0x8A, 0x21, 0x3A, 0x36, 0x15, 0x29, 0x2C, 0x79 };
        params[0] = OSData::withBytes(uuid, sizeof(uuid));
        params[1] = OSNumber::withNumber(0ULL, 32);
        params[2] = OSNumber::withNumber(1ULL, 32);
        params[3] = OSArray::withCapacity(0);

        OSObject *dsmResult = nullptr;
        IOReturn ret = acpiDev->evaluateObject("_DSM", &dsmResult, params, 4, 0);
        if (ret == kIOReturnSuccess && dsmResult) {
            IOLog("✅ FakeIrisXEFramebuffer::_DSM OK: %s\n", dsmResult->getMetaClass()->getClassName());
            dsmResult->release();
        } else {
            IOLog("❌ FakeIrisXEFramebuffer::_DSM failed: 0x%x\n", ret);
        }

        for (int i = 0; i < 4; i++) if (params[i]) params[i]->release();
    } else {
        IOLog("❌ FakeIrisXEFramebuffer::start() - GFX0 ACPI parent not found\n");
    }


    
    
    
    //BAR0 mapping
    IOMemoryMap *mmioMap = pciDevice->mapDeviceMemoryWithIndex(0);
    if (!mmioMap) {
        IOLog("FakeIrisXEFramebuffer::start() - BAR0 mapping failed\n");
        return false;
    }

    IOLog("FakeIrisXEFramebuffer::start() - BAR0 OK, length = %llu\n", mmioMap->getLength());
    volatile uint32_t *mmio = (volatile uint32_t *) mmioMap->getVirtualAddress();
    IOLog("FakeIrisXEFramebuffer::start() - Mapped MMIO VA: %p\n", mmio);
    
    
    
    
  
/*
    // --- PCI fix ---
    uint16_t pmcsr = pciDevice->configRead16(0x84);
    IOLog("FakeIrisXEFramebuffer::start() - PCI PMCSR before = 0x%04X\n", pmcsr);
    pmcsr &= ~0x3; // Force D0
    pciDevice->configWrite16(0x84, pmcsr);
    IOSleep(5);
    pmcsr = pciDevice->configRead16(0x84);
    IOLog("FakeIrisXEFramebuffer::start() - PCI PMCSR after force = 0x%04X\n", pmcsr);

    // --- New safe GT power + forcewake ---
    const uint32_t FORCEWAKE_MT = 0xA188;
   // const uint32_t FORCEWAKE_ACK = 0x130040;
    // const uint32_t FORCEWAKE_ACK_MEDIA = 0x130044;
    const uint32_t GT_PG_ENABLE = 0xA218;
    //const uint32_t PWR_WELL_CTL = 0x45400;
    
    
    
//GT_PG_ENABLE
    uint32_t pg_enable = mmio[GT_PG_ENABLE / 4];
    IOLog("GT_PG_ENABLE before: 0x%08X\n", pg_enable);
    mmio[GT_PG_ENABLE / 4] = pg_enable & ~0x1;
    IOSleep(5);
    uint32_t pg_enable_after = mmio[GT_PG_ENABLE / 4];
    IOLog("GT_PG_ENABLE after: 0x%08X\n", pg_enable_after);
    
    // Read FUSE_CTRL
    uint32_t fuse_ctrl = mmio[0x42000 / 4];
    IOLog("FakeIrisXEFramebuffer::start() - FUSE_CTRL before: 0x%08X\n", fuse_ctrl);

    // Try force bit 0 ON
    mmio[0x42000 / 4] = fuse_ctrl | 0x1;
    IOSleep(10);
    uint32_t fuse_ctrl_after = mmio[0x42000 / 4];
    IOLog("FakeIrisXEFramebuffer::start() - FUSE_CTRL after: 0x%08X\n", fuse_ctrl_after);

    // 1. PUNIT handshake: disable GT power gating
    const uint32_t PUNIT_PG_CTRL = 0xA2B0;
    uint32_t punit_pg = mmio[PUNIT_PG_CTRL / 4];
    IOLog("FakeIrisXEFramebuffer::start() - PUNIT_PG_CTRL before: 0x%08X\n", punit_pg);
    
    
    // ✅ Fix: Disable GT power gating by clearing bit 31
    punit_pg &= ~0x80000000;
    mmio[PUNIT_PG_CTRL / 4] = punit_pg;
    IOSleep(10);

    uint32_t punit_pg_after = mmio[PUNIT_PG_CTRL / 4];
    IOLog("FakeIrisXEFramebuffer::start() - PUNIT_PG_CTRL after: 0x%08X\n", punit_pg_after);
    


    // 2. Render Power Well ON
    const uint32_t PWR_WELL_CTL = 0x45400;
    uint32_t pw_ctl = mmio[PWR_WELL_CTL / 4];
    IOLog("FakeIrisXEFramebuffer::start() - Forcing Render PWR_WELL_CTL ON: before: 0x%08X\n", pw_ctl);

    // Set BIT(1) = Render Well
    mmio[PWR_WELL_CTL / 4] = pw_ctl | 0x2;
    IOSleep(10);
    
    // Enable Power Well 2 also
    mmio[PWR_WELL_CTL / 4] |= 0x4; // Bit 2 = PW2
    IOSleep(10);
    IOLog("PWR_WELL_CTL now: 0x%08X\n", mmio[PWR_WELL_CTL / 4]);

    uint32_t pw_ctl_after = mmio[PWR_WELL_CTL / 4];
    IOLog("FakeIrisXEFramebuffer::start() - Forcing Render PWR_WELL_CTL ON: after: 0x%08X\n", pw_ctl_after);

    // GT0: Wait for power well to be fully ON
    uint32_t pw_status = mmio[0x45408 / 4];
    IOLog("PWR_WELL_CTL_STATUS = 0x%08X\n", pw_status);
    
    // GEN11 render power domain: try unlocking manually
    const uint32_t GEN11_PWR_DOMAIN_MASK = 0x10000;
    mmio[0xA278 / 4] |= GEN11_PWR_DOMAIN_MASK;
    IOSleep(10);

    // Enable GT thread dispatch (test)
    mmio[0x138128 / 4] = 0x00000001;
    IOSleep(5);

    
    IOLog("Trying FORCEWAKE Render domain\n");
    // Try FORCEWAKE_ALL
    mmio[FORCEWAKE_MT / 4] = 0x000F000F;  // all domains
    IOSleep(10);
    uint32_t ack_all = mmio[FORCEWAKE_ACK / 4];
    IOLog("FORCEWAKE_ACK (Global): 0x%08X\n", ack_all);


    if ((ack_all & 0x1) == 0) {
        IOLog("Trying FORCEWAKE Media domain\n");
        mmio[FORCEWAKE_MT / 4] = 0x00020002;
        IOSleep(5);
        uint32_t media_ack = mmio[FORCEWAKE_ACK_MEDIA / 4];
        IOLog("FORCEWAKE_ACK (Media): 0x%08X\n", media_ack);
    }
    
    // New: try force dummy read to latch Render domain
    IOLog("Trying dummy read after FORCEWAKE Render write\n");
    volatile uint32_t dummy = mmio[FORCEWAKE_MT / 4];
    IOSleep(5);
    uint32_t ack_force = mmio[FORCEWAKE_ACK / 4];
    IOLog("FORCEWAKE_ACK (Render) after dummy read: 0x%08X\n", ack_force);

    
    
    //  Try to ping Render domain again with loop
    IOLog("Re-trying FORCEWAKE Render domain after Media wake\n");
    mmio[FORCEWAKE_MT / 4] = 0x00010001;

    for (int i = 0; i < 1000; ++i) {
        uint32_t ack = mmio[FORCEWAKE_ACK / 4];
        if (ack & 0x1) {
            IOLog("FORCEWAKE_ACK (Render) now set! 0x%08X\n", ack);
            break;
        }
        IOSleep(1);
        if (i == 999) {
            IOLog("FORCEWAKE_ACK (Render) still not set, final: 0x%08X\n", ack);
        }
    }
    
    // Extra: Poke GT Thread Status to nudge Render domain awake
    const uint32_t GT_THREAD_STATUS = 0x138124;
    uint32_t gt_thread = mmio[GT_THREAD_STATUS / 4];
    IOLog("GT_THREAD_STATUS before poke: 0x%08X\n", gt_thread);

    // Sometimes poking the first bit wakes up Render domain
    mmio[GT_THREAD_STATUS / 4] = gt_thread | 0x1;
    IOSleep(5);
    uint32_t gt_thread_after = mmio[GT_THREAD_STATUS / 4];
    IOLog("GT_THREAD_STATUS after poke: 0x%08X\n", gt_thread_after);

    // Re-read FORCEWAKE_ACK Render
    uint32_t ack_retry = mmio[FORCEWAKE_ACK / 4];
    IOLog("FORCEWAKE_ACK (Render) after GT poke: 0x%08X\n", ack_retry);

    uint32_t mmio_test = mmio[0];
    IOLog("First DWORD after handshake: 0x%08X\n", mmio_test);
    
    
    // -------------------------------------------------------------
    // 🧪 Try FORCEWAKE_REQ as alternate to FORCEWAKE_MT
    // -------------------------------------------------------------
    IOLog("→ Trying FORCEWAKE_REQ for Render domain\n");
    mmio[0xA188 / 4] = 0x00010001;  // FORCEWAKE_REQ register for Render
    IOSleep(10);
    uint32_t ackRenderREQ = mmio[FORCEWAKE_ACK_RENDER / 4];
    IOLog("→ FORCEWAKE_ACK (Render) after REQ write: 0x%08X\n", ackRenderREQ);

    // -------------------------------------------------------------
    // 🧪 Try legacy FORCEWAKE registers (pre-Gen9 compatibility)
    // -------------------------------------------------------------
    IOLog("→ Trying legacy FORCEWAKE request\n");
    mmio[0xA008 / 4] = 0x00010001;  // Legacy FORCEWAKE Request
    IOSleep(10);
    uint32_t legacyAck = mmio[0xA00C / 4];
    IOLog("→ Legacy FORCEWAKE_ACK: 0x%08X\n", legacyAck);


    // Example: dump small MMIO window
    for (uint32_t offset = 0; offset < 0x40; offset += 4) {
        uint32_t val = mmio[offset / 4];
        IOLog("MMIO[0x%04X] = 0x%08X\n", offset, val);
    }
    
    
    // ---------- EXTRA: Attempt to unlock GT domains ----------

    // 1. Disable GT Clock Gating (optional but safe)
    const uint32_t GT_CLOCK_GATE_DISABLE = 0x09400;
    mmio[GT_CLOCK_GATE_DISABLE / 4] = 0xFFFFFFFF;
    IOLog("GT_CLOCK_GATE_DISABLE set\n");

    // 2. Disable RC6 sleep states (optional, may help bring up Render)
    const uint32_t RC6_CONTROL = 0x08500;
    mmio[RC6_CONTROL / 4] = 0;
    IOLog("RC6_CONTROL cleared (RC6 disabled)\n");

    // 3. Dump ECOBUS (debug sanity check)
    const uint32_t ECOBUS = 0x0A180;
    uint32_t ecobus = mmio[ECOBUS / 4];
    IOLog("ECOBUS = 0x%08X\n", ecobus);
    
    
    mmio[0xA248 / 4] = 0xFFFFFFFF;  // Enable all power domains
    mmio[0x9400 / 4] = 0x00000000;  // Disable clock gating
    mmio[0x4E100 / 4] = 0x00000000;  // Memory latency tolerance
    mmio[0x4E104 / 4] = 0x000000FF;  // Memory bandwidth allocation
    
  */
    
    
    // === GPU Acceleration Properties ===
    {
        // Required properties for Quartz Extreme / Core Animation
        OSArray* accelTypes = OSArray::withCapacity(2);
        if (accelTypes) {
            accelTypes->setObject(OSString::withCString("Accel"));
            accelTypes->setObject(OSString::withCString("Metal"));
            setProperty("IOAccelTypes", accelTypes);
            accelTypes->release(); // done after setProperty retains it
        
            IOLog("GPU Acceleration Properties used");
        }

    }

    
    //child display
    IOService* childDisplay = new IOService;
    if (childDisplay && childDisplay->init()) {
        childDisplay->setName("display0");

        // ✅ Use full path to override .Display_boot/display0/AppleDisplay
        const char* spoofedPrefsKey = "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/GFX0@2/display0";
        OSString* displayPrefsKey = OSString::withCString(spoofedPrefsKey);
        if (displayPrefsKey) {
            childDisplay->setProperty("IODisplayPrefsKey", displayPrefsKey);
            displayPrefsKey->release();
        }

        // ✅ Spoof identity: product/vendor/serial
        childDisplay->setProperty("DisplayProductID", (uint32_t)0x717);
        childDisplay->setProperty("DisplayVendorID", (uint32_t)0x756e6b6e);
        childDisplay->setProperty("DisplaySerialNumber", (uint32_t)0);

        childDisplay->setProperty("IODisplayLocation", "Built-in");
        childDisplay->setProperty("IOFramebufferDisplayIndex", OSNumber::withNumber((uint32_t)0, 32));
        childDisplay->setProperty("IOFBDependentIndex", OSNumber::withNumber((uint32_t)0, 32));
        childDisplay->setProperty("IOFBDependentID", kOSBooleanTrue);
        childDisplay->setProperty("IOFramebufferDisplay", true);
        childDisplay->setProperty("IOUserClientClass", "IOFramebufferUserClient");
        setProperty("IOFramebufferSharedUserClient", true);
        childDisplay->attach(this);

        IOLog("✅ Published spoofed display0 with hijacked IODisplayPrefsKey\n");
    }
    
    
    OSDictionary* singleDisplay = OSDictionary::withCapacity(1);
    singleDisplay->setObject("IODisplayPrefsKey", OSString::withCString("IOService:/AppleACPIPlatformExpert/PC00@0/AppleACPIPCI/GFX0@2/display0"));

    OSArray* displayList = OSArray::withCapacity(1);
    displayList->setObject(singleDisplay);

    OSDictionary* fbDisplays = OSDictionary::withCapacity(1);
    fbDisplays->setObject("display0", displayList);

    setProperty("IOFramebufferDisplays", fbDisplays);

    // CLEANUP (prevent leaks)
    singleDisplay->release();
    displayList->release();
    fbDisplays->release();

    IOLog("✅ Proper IOFramebufferDisplays dictionary published\n");



    
    
    // Constants
    const uint32_t width = 1920;
    const uint32_t height = 1080;
    const uint32_t bpp = 4; // bytes per pixel (ARGB8888)
    const uint32_t fbSize = round_page(width * height * bpp); // align to page

    IOLog("🧠 Allocating framebuffer memory: %ux%u, %u bytes\n", width, height, fbSize);

    // Allocate framebuffer memory (shared, aligned)
    framebufferMemory = IOBufferMemoryDescriptor::withOptions(
        kIOMemoryKernelUserShared | kIODirectionInOut,
        fbSize,
        page_size // 4096
    );

    // Validate
    if (!framebufferMemory) {
        IOLog("❌ Failed to allocate framebuffer memory\n");
        return false;
    }

    // Prepare for DMA access
    if (framebufferMemory->prepare() != kIOReturnSuccess) {
        IOLog("❌ Failed to prepare framebuffer memory\n");
        framebufferMemory->release();
        framebufferMemory = nullptr;
        return false;
    }

    // Zero the memory (safely)
    void* fbAddr = framebufferMemory->getBytesNoCopy();
    if (fbAddr) bzero(fbAddr, fbSize);

    IOLog("✅ Framebuffer allocated and initialized successfully\n");


    
    
    /*
    deliverFramebufferNotification(0, kIOFBNotifyDisplayAdded, nullptr);
    deliverFramebufferNotification(0, kIOFBConfigChanged, nullptr);
     
    // Create work loop and command gate
       workLoop = IOWorkLoop::workLoop();
       if (!workLoop) {
           IOLog("Failed to create work loop\n");
           return false;
       }
       
       commandGate = IOCommandGate::commandGate(this);
       if (!commandGate || workLoop->addEventSource(commandGate) != kIOReturnSuccess) {
           IOLog("Failed to create command gate\n");
           return false;
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
    setProperty("IOTimingInformation", timingInfo);
    timingInfo->release();
    
    
    

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
            setProperty("IODisplayEDID", edidData);
            edidData->release();
            IOLog("📺 Fake EDID published\n");
        }

        setProperty("IOFBHasPreferredEDID", kOSBooleanTrue);
    }

    
/*
    // VSync simulation
    vsyncSource = IOInterruptEventSource::interruptEventSource(
        this,
        OSMemberFunctionCast(IOInterruptEventAction, this, &FakeIrisXEFramebuffer::vsyncOccurred)
    );

    if (vsyncSource) {
        getWorkLoop()->addEventSource(vsyncSource);
    }

    vsyncTimer = IOTimerEventSource::timerEventSource(
        this,
        OSMemberFunctionCast(IOTimerEventSource::Action, this, &FakeIrisXEFramebuffer::vsyncTimerFired)
    );

    if (vsyncTimer) {
        getWorkLoop()->addEventSource(vsyncTimer);
        vsyncTimer->setTimeoutMS(16); // 60Hz = 16.67ms
    }

  */
    
    
    
    //extra property
    setProperty("AAPL,boot-display", kOSBooleanTrue);
    setProperty("IONameMatched", "GFX0");
    setProperty("IOFBHasBacklight", kOSBooleanTrue);
    setProperty("IOFBBacklightDisplay", kOSBooleanTrue);

    setProperty("IOFBUserClientClass", "IOFramebufferUserClient");
    setProperty("IOAccelRevision", 2, 32);
    setProperty("IOAccelVRAMSize", 128 * 1024 * 1024, 128);
    setProperty("IOFBNeedsRefresh", kOSBooleanTrue);
    setProperty("IOFBConfig", 1, 32);
    setProperty("IOFBDisplayModeID", OSNumber::withNumber((UInt64)0, 32)); // Default mode
    setProperty("IOFBStartupModeTimingID", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBCurrentPixelFormat", "ARGB8888");
    setProperty("IODisplayParameters", OSDictionary::withCapacity(5));
    setProperty("IOPMFeatures", OSDictionary::withCapacity(2));
    setProperty("IOFBCursorInfo", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBTransform", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBGammaHeaderSize", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBWaitCursorFrames", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBScalerInfo", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFBClientConnectIndex", 1, 32);
    setProperty("IOUserClientCreator", "WindowServer");
    setProperty(kIOFBCursorSupportedKey, kOSBooleanFalse);
    setProperty(kIOFBHardwareCursorSupportedKey, kOSBooleanFalse);
    setProperty("IOFBMemorySize", framebufferMemory->getLength(), 32); // Safe
    setProperty(kIOFBScalerInfoKey, 0ULL, 32);
    setProperty(kIOFBDisplayModeCountKey, static_cast<UInt64>(1), 32);
    setProperty("IOFramebufferOpenGLIndex", 0ULL, 32);
    setProperty("IOFBCurrentPixelCount", 1920ULL * 1080ULL, 128);
    setProperty("IOFBMemorySize", vramSize, 128);
    setProperty("IOFBCursorScale", 0x10000, 32); // 1.0 fixed-point
    setProperty("IOFBDisplayCount", (uint32_t)1);
    setProperty("IOFBDependentID", kOSBooleanTrue);
    setProperty("IOFBDependentIndex", (uint32_t)0);
    setProperty("IOFBVerbose", kOSBooleanTrue);
    setProperty("IOFBTranslucencySupport", kOSBooleanTrue);
    setProperty("IOGVAHEVCEncode", kOSBooleanTrue);
    setProperty("IOGVAVTEnable", kOSBooleanTrue);
    setProperty("IOSupportsCLUTs", kOSBooleanTrue);
    setProperty("IOAccelEnabled", kOSBooleanTrue);
    setProperty("AAPL,HasPanel", kOSBooleanTrue);
    setProperty("built-in", kOSBooleanTrue);
    setProperty("AAPL,gray-page", kOSBooleanTrue);
    setProperty("IOFBTransparency", kOSBooleanTrue);
    setProperty("IOSurfaceSupport", kOSBooleanTrue);
    setProperty("IOSurfaceAccelerator", kOSBooleanTrue);
    setProperty("IOGraphicsHasAccelerator", kOSBooleanTrue);
    setProperty("IOProviderClass", "IOPCIDevice");

    setProperty("IOAccelIndex", static_cast<unsigned int>(0), 32);
  
    setProperty("MetalPluginName", OSSymbol::withCString("AppleIntelICLLPGraphicsMTLDriver"));

    setProperty("VRAM,totalsize", OSNumber::withNumber(128 * 1024 * 1024, 32)); // 128MB

    setProperty("IOGVAHEVCDecode", kOSBooleanTrue);
    setProperty("IOGVAVTDecodeSupport", kOSBooleanTrue);
    setProperty("IOVARendererID", OSNumber::withNumber(0x80860100, 32));
    
    setProperty("IOFBConnectFlags", kIOConnectionBuiltIn, 32);
    setProperty("IOFBCurrentConnection", currentConnection, 32);
    setProperty("IOFBOnline", 1, 32);
    setProperty("model", OSData::withBytes("Intel Iris Xe Graphics", 22));
    setProperty("IOFramebufferDisplayIndex", (uint32_t)0);
    setProperty("IOFBStartupDisplayModeTimingID", OSNumber::withNumber((UInt64)0, 32));
    setProperty("IOFramebufferDisplay", kOSBooleanTrue);     // Very important

    
    OSDictionary* fbInfo = OSDictionary::withCapacity(1);
    fbInfo->setObject("FramebufferType", OSSymbol::withCString("IntelIrisXe"));
    setProperty("IOFramebufferInformation", fbInfo);
    fbInfo->release();
    
    
    OSArray* pixelFormats = OSArray::withCapacity(2);
    pixelFormats->setObject(OSSymbol::withCString("ARGB8888"));
    pixelFormats->setObject(OSSymbol::withCString("RGBA8888"));
    setProperty("IOFBSupportedPixelFormats", pixelFormats);
    pixelFormats->release();

    

    // Cursor sizes
    OSNumber* cursorSize = OSNumber::withNumber(32, 32);
    if (cursorSize) {
        OSObject* values[1] = { cursorSize };
        OSArray* array = OSArray::withObjects((const OSObject**)values, 1);
        if (array) {
            setProperty("IOFBCursorSizes", array);
            array->release();
        }
        cursorSize->release();
    }

    setProperty("IOFBNumberOfConnections", 1, 32);
    setProperty("IOFBConnectionFlags", kIOConnectionBuiltIn, 32);
    
    

    
    
    registerService();
    

        
        
IOLog("FakeIrisXEFramebuffer::start() - Completed\n");
return true;

}


void FakeIrisXEFramebuffer::stop(IOService* provider) {
    IOLog("FakeIrisXEFramebuffer::stop() called\n");
    
    if (mmioMap) {
            mmioMap->release();
            mmioMap = nullptr;
        }
    
    if (vramMemory) {
        OSSafeReleaseNULL(vramMemory); // avoids double free
    }
    
        super::stop(provider);
}



void FakeIrisXEFramebuffer::startIOFB() {
    IOLog("FakeIrisXEFramebuffer::startIOFB() called\n");
    deliverFramebufferNotification(0, kIOFBNotifyDisplayModeChange, nullptr); // This is enough

}


void FakeIrisXEFramebuffer::free() {
    IOLog("FakeIrisXEFramebuffer::free() called\n");
    
    
    if (commandGate) {
        workLoop->removeEventSource(commandGate);
        commandGate->release();
        commandGate = nullptr;
    }
    if (workLoop) {
        workLoop->release();
        workLoop = nullptr;
    }

    super::free();
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

    vramMemory = IOBufferMemoryDescriptor::withOptions(
        kIODirectionInOut | kIOMemoryKernelUserShared,
        vramSize,
        PAGE_SIZE);
    
    if (!vramMemory) {
        IOLog("Failed to allocate VRAM\n");
        return false;
    }
    
    // Clear screen to black
    memset(vramMemory->getBytesNoCopy(), 0, vramSize);
    
    return true;
}


IOReturn FakeIrisXEFramebuffer::enableController() {
    IOLog("enableController() called\n");
    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::getOnlineState(IOIndex connectIndex, bool* online)
{
    if (connectIndex != 0) return kIOReturnBadArgument;
    *online = displayOnline;
    IOLog("getOnlineState: %d\n", displayOnline);
    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::setOnlineState(IOIndex connectIndex, bool online)
{
    if (connectIndex != 0) return kIOReturnBadArgument;


    IOLog("setOnlineState: %d\n", online);
    return kIOReturnSuccess;
}


bool FakeIrisXEFramebuffer::setupDisplayModes() {
    IOLog("setupDisplayModes() called\n");
    currentMode = 0;
    currentDepth = 0;

    setNumberOfDisplays(1);
    
    return true;
}




bool FakeIrisXEFramebuffer::isConsoleDevice() {
    return true; // Claim to be console device
}

IOReturn FakeIrisXEFramebuffer::setOnline(bool online) {

    IOLog("setOnline() called\n");
    displayOnline = online;
    return online ? kIOReturnSuccess : kIOReturnUnsupported;
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
    bzero(infoOut, sizeof(IOTimingInformation));
    infoOut->appleTimingID = kIOTimingIDDefault;
    infoOut->flags = kIOTimingInfoValid_AppleTimingID;
    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::setCLUTWithEntries(IOColorEntry* colors,
                                                   UInt32 firstIndex,
                                                   UInt32 numEntries,
                                                   IOOptionBits options) {
    IOLog("setCLUTWithEntries() called\n");

    return kIOReturnUnsupported;
}
    

IOReturn FakeIrisXEFramebuffer::setGammaTable(UInt32 channelCount,
                                              UInt32 dataCount,
                                              UInt32 dataWidth,
                                              void* data) {
    IOLog("setGammaTable() called\n");

    return kIOReturnUnsupported;
}

IOReturn FakeIrisXEFramebuffer::getGammaTable(UInt32 channelCount,
                                              UInt32* dataCount,
                                              UInt32* dataWidth,
                                              void** data) {
    IOLog("getGammaTable() called\n");
    return kIOReturnUnsupported;
}


IOReturn FakeIrisXEFramebuffer::getAttributeForConnection(IOIndex connectIndex,
                                                         IOSelect attribute,
                                                         uintptr_t* value)
{
    IOLog("getAttributeForConnection(%d, 0x%x)\n", connectIndex, attribute);
    
    switch (attribute) {
        case kConnectionSupportsAppleSense:
        case kConnectionSupportsLLDDCSense:
        case kConnectionSupportsHLDDCSense:
        case kConnectionSupportsDDCSense:
        case kIOCapturedAttribute:
                   *value = 0;
                   return kIOReturnSuccess;
                   
               case kIOHardwareCursorAttribute:
                   *value = 1; // Report cursor support
                   return kIOReturnSuccess;
            
        case kConnectionFlags:
            *value = kIOConnectionBuiltIn | kIOConnectionDisplayPort;
            return kIOReturnSuccess;
            
            
        case kConnectionEnable:
                    *value = 1;  // Connection enabled
                    return kIOReturnSuccess;
            
        default:
            return kIOReturnUnsupported;
    }
}



IOReturn FakeIrisXEFramebuffer::getDisplayModes(IODisplayModeID* allDisplayModes)
{
    if (allDisplayModes)
        allDisplayModes[0] = 0; // Safe fallback

    return kIOReturnSuccess;
}



const char* FakeIrisXEFramebuffer::getPixelFormats() {
    static const char formats[] =
        IO32BitDirectPixels "\0"
        "\0";
    return formats;
}

    
    
IOReturn FakeIrisXEFramebuffer::setCursorImage(void* cursorImage) {
    return kIOReturnSuccess;
}

IOReturn FakeIrisXEFramebuffer::setCursorState(SInt32 x, SInt32 y, bool visible) {
    return kIOReturnSuccess;
}

IOReturn FakeIrisXEFramebuffer::registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc, void* ref, void** interruptRef) {
    
    return kIOReturnSuccess;
}

IOReturn FakeIrisXEFramebuffer::unregisterInterrupt(void* interruptRef) {
    return kIOReturnSuccess;
}




IOReturn FakeIrisXEFramebuffer::createAccelTask(mach_port_t* port) {

    return kIOReturnUnsupported;
}



void FakeIrisXEFramebuffer::vsyncTimerFired(OSObject* owner, IOTimerEventSource* sender)
{
    // Trigger vsync interrupt
    if (vsyncSource) {
        vsyncSource->interruptOccurred(nullptr, nullptr, 0);
    }
    // Reschedule
    vsyncTimer->setTimeoutMS(16);
}

void FakeIrisXEFramebuffer::vsyncOccurred(OSObject* owner, IOInterruptEventSource* src, int count)
{
    // Notify WindowServer
    deliverFramebufferNotification(0, kIOFBVsyncNotification, nullptr);
}





IOReturn FakeIrisXEFramebuffer::setDisplayMode(IODisplayModeID mode, IOIndex depth) {
    if (mode != 0 || depth != 0)
        return kIOReturnUnsupportedMode;

    currentMode = mode;
    currentDepth = depth;
    return kIOReturnSuccess;
}



IODeviceMemory* FakeIrisXEFramebuffer::getVRAMRange()
{
    IOLog("FakeIrisXEFramebuffer::getVRAMRange()\n");

    if (!framebufferMemory)
        return nullptr;

    IOMemoryMap* map = framebufferMemory->map();
    if (!map) {
        IOLog("❌ Could not map framebuffer memory\n");
        return nullptr;
    }

    IOPhysicalAddress physAddr = map->getPhysicalAddress();

    IODeviceMemory *deviceMem = IODeviceMemory::withRange(physAddr, framebufferMemory->getLength());
    if (!deviceMem)
        IOLog("getVRAMRange(): withRange failed\n");

    return deviceMem;
}



IOReturn FakeIrisXEFramebuffer::createSharedCursor(
    IOIndex /*connectIndex*/,
    int /*version*/) {
    IOLog("createSharedCursor() called\n");
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





IOReturn FakeIrisXEFramebuffer::clientMemoryForType(
    UInt32 type,
    UInt32* flags,
    IOMemoryDescriptor** memory)
{
    IOLog("FakeIrisXEFramebuffer::clientMemoryForType() - type: %u\n", type);

    if (type == kIOFBCursorMemory && cursorMemory) {
        cursorMemory->retain();
        *memory = cursorMemory;
        if (flags) *flags = 0;
        return kIOReturnSuccess;
    }

    if (type == kIOFBVRAMMemory && framebufferSurface) {
        framebufferSurface->retain();
        *memory = framebufferSurface;
        if (flags) *flags = 0;
        return kIOReturnSuccess;
    }

    
    
    return kIOReturnUnsupported;
}





IOReturn FakeIrisXEFramebuffer::flushDisplay(void)
{
    IOLog("🌀 flushDisplay() called - frame committed\n");
    return kIOReturnSuccess;
}

void FakeIrisXEFramebuffer::deliverFramebufferNotification(IOIndex index, UInt32 event, void* info) {
    IOLog("📩 deliverFramebufferNotification() index=%u event=0x%08X\n", index, event);
    super::deliverFramebufferNotification(index, info);
}






IOReturn FakeIrisXEFramebuffer::setNumberOfDisplays(UInt32 count)
{
    
    
    IOLog("🖥️ setNumberOfDisplays(%u)\n", count);
    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::flushFramebuffer() {
    IOLog("flushFramebuffer() called\n");
    
    return kIOReturnSuccess;
}



IOReturn FakeIrisXEFramebuffer::setPowerState(unsigned long whichState, IOService* whatDevice) {
    IOLog("FakeIrisXEFramebuffer::setPowerState(%lu)\n", whichState);

    if (!mmioMap) {
        IOLog("FakeIrisXEFramebuffer::setPowerState: mmio is null\n");
        return kIOReturnError;
    }

    IOLog("Power state change requested: %lu — MMIO writes disabled during testing\n", whichState);

    // Avoid MMIO writes for now
    // volatile uint32_t* mmio = (volatile uint32_t*)mmioMap->getVirtualAddress();
    // mmio[0xA248/4] = (whichState == 0) ? 0x00000000 : 0xFFFFFFFF;
    // mmio[0xA188/4] = 0x00010001;

    return kIOPMAckImplied;
}




IOItemCount FakeIrisXEFramebuffer::getDisplayModeCount(void)
{
    return 1; // Must return at least 1 mode
}




UInt64 FakeIrisXEFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
    IOLog("getPixelFormatsForDisplayMode() called\n");
    return 0; // obsolete — always return 0
}


IOReturn FakeIrisXEFramebuffer::getPixelInformation(
    IODisplayModeID displayMode,
    IOIndex depth,
    IOPixelAperture aperture,
    IOPixelInformation* info)
{
    if (!info) return kIOReturnBadArgument;
    if (displayMode != 1 || depth != 0) return kIOReturnUnsupportedMode;

    bzero(info, sizeof(IOPixelInformation));
    strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat)); // 'ARGB'

    info->flags             = 0;
    info->pixelType         = kIORGBDirectPixels; // ✅ correct type
    info->componentCount    = 4;                  // ✅ ARGB = 4
    info->bitsPerComponent  = 8;
    info->bitsPerPixel      = 32;
    info->bytesPerRow       = 1920 * 4;           // ✅ correct pitch
    info->activeWidth       = 1920;
    info->activeHeight      = 1080;

    IOLog("✅ getPixelInformation(): ARGB8888, 1920x1080, 32bpp\n");

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

    // Assume you have a variable like this set up:
    // IODeviceMemory* vramRange = IODeviceMemory::withRange(mmioBase, vramSize);
    return getVRAMRange();  // <-- your own helper that returns IODeviceMemory*
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

    // In a real scenario, you'd configure the hardware here based on displayMode.
    // For now, just track the current mode.
    currentMode = displayMode;
    currentDepth = depth;

    // Example of setting a property if you want to reflect the chosen pixel format
    if (depth == 32) { // Assuming 32-bit depth means ARGB8888
        setProperty("IOFBCurrentPixelFormat", OSString::withCString("ARGB8888"));
    } else if (depth == 16) { // Assuming 16-bit depth means RGB565
        setProperty("IOFBCurrentPixelFormat", OSString::withCString("RGB565"));
    }

    // Return success even if you don't fully configure hardware yet.
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
    info->flags = 0;
    info->reserved[0] = 0;
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
    
    if (attribute == kIOCaptureAttribute) {
            IOLog("✅ Received kIOCaptureAttribute\n");
            return kIOReturnSuccess;
        }
        return IOFramebuffer::setAttribute(attribute, value);
    }




#include <libkern/libkern.h> // For kern_return_t, kmod_info_t

// Kext entry/exit points
extern "C" kern_return_t FakeIrisXEFramebuffer_start(kmod_info_t *ki, void *data) {
    return KERN_SUCCESS;
}

extern "C" kern_return_t FakeIrisXEFramebuffer_stop(kmod_info_t *ki, void *data) {
    return KERN_SUCCESS;
}
