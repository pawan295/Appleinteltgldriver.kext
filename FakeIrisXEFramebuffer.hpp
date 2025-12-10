/* #ifndef _FAKE_IRIS_XE_FRAMEBUFFER_HPP_ */
/* #define _FAKE_IRIS_XE_FRAMEBUFFER_HPP_ */
#pragma once


#include <IOKit/acpi/IOACPIPlatformDevice.h>

#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/pci/IOPCIDevice.h> // for IOPCIDevice
#include <IOKit/IOBufferMemoryDescriptor.h> // for IODeviceMemory
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/graphics/IODisplay.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <libkern/OSAtomic.h>
// OR (for newer versions)
#include <os/atomic.h>

#include "FakeIrisXEGEM.hpp"
#include "FakeIrisXEExeclist.hpp"

#include "FakeIrisXERing.h"

#include "FakeIrisXEBacklight.hpp" // include so the compiler sees the type

#include <IOKit/IOLocks.h>










class FakeIrisXEFramebuffer : public IOFramebuffer

{
    OSDeclareDefaultStructors(FakeIrisXEFramebuffer)
    

private:
    

    
    IOInterruptEventSource* vsyncSource;
    IOTimerEventSource* vsyncTimer = nullptr;
    IODisplayModeID supportedModes[1] = {0};
    IOTimerEventSource* displayInjectTimer = nullptr;
    IOWorkLoop* workLoop = nullptr;

    volatile bool driverActive;
        IOLock* timerLock;
    
    // Cursor state
    SInt32 cursorX, cursorY;
    bool cursorVisible;
    
    // Color management
    IOColorEntry* clutTable;
    void* gammaTable;
    size_t gammaTableSize;
    
    // Interrupt handling
    struct InterruptInfo {
        IOSelect type;
        IOFBInterruptProc proc;
        void* ref;
    };
    OSArray* interruptList;
    
    void activatePowerAndController();
    
    bool initPowerManagement();

    volatile void *  wsFrontBuffer = nullptr;

      // simple gamma cache
      struct { bool set=false; UInt16 table[256*3]; } gamma{};

      // helpers
      void waitVBlank();       // simple vblank poll
    
    
    void* fFB;
    
    
    
protected:
 
    IOPhysicalAddress bar0Phys = 0;

    
    
    IOMemoryMap* bar0Map;

    enum {
        kConnectionEnable              = 0x656E6162, // 'enab'
        kConnectionDisable             = 0x646E626C, // 'dnbl'
        kConnectionHandleDisplayConfig = 0x68646463, // 'hddc'
        kConnectionFlush               = 0x666C6773, // 'flgs'
        kConnectionLinkChanged         = 0x6C636863, // 'lchc'
        kConnectionSupportsFastSwitch  = 0x73777368, // 'swsh'
        kConnectionAssign              = 0x61736E73, // 'asns'
        kConnectionEnableAudio         = 0x656E6175, // 'enau'
    };

    
    
    IOMemoryMap* vramMap;
    IOMemoryMap* mmioMap;
    IOBufferMemoryDescriptor* vramMemory;
        IOPCIDevice* pciDevice;
        volatile UInt8* mmioBase;
        IODisplayModeID currentMode;
        IOIndex currentDepth;
        size_t vramSize;
    
    IOMemoryDescriptor* framebufferSurface;

    IOBufferMemoryDescriptor* cursorMemory;

    
         
    IOIndex currentConnection;
      bool displayOnline;
    bool fullyInitialized = false;
    IOLock* powerLock;
    bool shuttingDown = false;


    
    
    
    
    
 public:
    
    
    // Safe MMIO access methods
       uint32_t safeMMIORead(uint32_t offset) {
           if (!mmioBase || offset >= mmioMap->getLength()) {
               IOLog("Invalid MMIO read at 0x%X\n", offset);
               return 0xFFFFFFFF;
           }
           return *(volatile uint32_t*)(mmioBase + offset);
       }
       
       void safeMMIOWrite(uint32_t offset, uint32_t value) {
           if (!mmioBase || offset >= mmioMap->getLength()) {
               IOLog("Invalid MMIO write at 0x%X\n", offset);
               return;
           }
           *(volatile uint32_t*)(mmioBase + offset) = value;
                  #ifdef OSMemoryBarrier
                  OSMemoryBarrier();
                  #else
                  __asm__ volatile("mfence" ::: "memory"); // x86 specific
                  #endif

       }
    
    
    
    
    
    bool fNeedFlush = false;   // <-- REQUIRED (you were missing this)

    
    IOCommandGate* commandGate;

    void scheduleFlushFromAccelerator(); // called from accelerator
       static IOReturn staticFlushAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
        
    
    bool mapFramebufferIntoGGTT();

    
    static constexpr uint32_t H_ACTIVE = 1920;
    static constexpr uint32_t V_ACTIVE = 1080;

    uint32_t getWidth()  const { return H_ACTIVE; }
     uint32_t getHeight() const { return V_ACTIVE; }
    uint32_t getStride() { return 7680; }

    
    
    
 
    
     uint64_t getFramebufferPhysAddr() const {
         return framebufferMemory ? framebufferMemory->getPhysicalAddress() : 0;
     }
    

    
    
//   uint64_t fFBPhys{0};

  
    
    void*    getFramebufferKernelPtr() const; // from IOBufferMemoryDescriptor::getBytesNoCopy()

    
    
    
    bool start(IOService* provider) override;
    void stop(IOService* provider) override;
    virtual bool           init(OSDictionary* dict) override;
    bool   setupWorkLoop();


    virtual void           free() override;
    virtual void startIOFB();

    
    
    bool controllerEnabled = false;

    bool displayPublished = false; // ✅ Member variable
    
    void disableController();
    
    
    enum {
        kPowerStateOff = 0,
        kPowerStateOn,
        kNumPowerStates
    };

    static IOPMPowerState powerStates[kNumPowerStates];


    
    
    virtual IOService *probe(IOService *provider, SInt32 *score) override;

    
    bool getIsUsable() const;
    
    
    virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice) override;

    
    virtual const char* getPixelFormats() override;
    
    virtual UInt64 getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
    virtual IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
   
 
    
    virtual UInt32 getConnectionCount() override;
    
    virtual IOReturn getStartupDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) override;
    virtual IOReturn getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t *value) override;



    virtual IOReturn createSharedCursor(IOIndex connectIndex, int version) ;
    virtual IOReturn setBounds(IOIndex index, IOGBounds *bounds) ;
        
    
    virtual IOReturn getDisplayModes(IODisplayModeID* allDisplayModes) override;

    
    virtual IOReturn getInformationForDisplayMode(IODisplayModeID mode, IODisplayModeInformation *info) override;
    

    virtual IOReturn getFramebufferOffsetForX_Y(IOPixelAperture aperture, SInt32 x, SInt32 y, UInt32 *offset) ;
    
    virtual IOReturn clientMemoryForType(UInt32 type,
                                             UInt32* flags,
                                             IOMemoryDescriptor** memory) ;

    virtual IOReturn enableController() override;
    

   virtual IOReturn getPixelInformation(IODisplayModeID displayMode,
                                 IOIndex depth,
                                 IOPixelAperture aperture,
                                 IOPixelInformation* info) override;

   virtual IOReturn getCurrentDisplayMode(IODisplayModeID* displayMode, IOIndex* depth) override;

    

    virtual IOReturn setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t value) override;
    
    virtual IOReturn flushDisplay(void) ;
    
    
    virtual void deliverFramebufferNotification(IOIndex index, UInt32 event, void* info);

    
     IOReturn setNumberOfDisplays(UInt32 count) ;
    
    
    
    virtual IODeviceMemory* getApertureRange(IOPixelAperture aperture) APPLE_KEXT_OVERRIDE;

    virtual IOReturn getApertureRange(IOSelect aperture,
                                      IOPhysicalAddress *phys,
                                      IOByteCount *length);

    
    
    
    
    
    virtual IOIndex getAperture() const ;

    
    
    virtual IOItemCount getDisplayModeCount(void) override;

    
        virtual IOReturn  getAttribute(IOSelect attr, uintptr_t *value) override;
    
        virtual IOReturn  setAttribute(IOSelect attribute, uintptr_t value) override;
        
        // Optional but recommended
        virtual IOReturn       registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc, void* ref, void** interruptRef) ;
    
        virtual IOReturn       unregisterInterrupt(void* interruptRef) override;
        virtual IOReturn       setCursorImage(void* cursorImage) override;
        virtual IOReturn       setCursorState(SInt32 x, SInt32 y, bool visible) override;

   virtual IOReturn getTimingInfoForDisplayMode(IODisplayModeID mode,
                                                IOTimingInformation* info)override;

    virtual IOReturn setGammaTable(UInt32 channelCount,
                                                      UInt32 dataCount,
                                                      UInt32 dataWidth,
                                                      void* data)override;
            
    
    
        
    
    virtual  IOReturn getGammaTable(UInt32 channelCount,
                                                  UInt32* dataCount,
                                                  UInt32* dataWidth,
                                                         void** data);

    // Essential methods for IOFramebufferUserClient to function
    virtual IOReturn getAttributeForIndex(IOSelect attribute, UInt32 index, UInt32* value) ;

   
 
    // in class FakeIrisXEFramebuffer : public IOService / IOFramebuffer...
    IOTimerEventSource* fVBlankTimer = nullptr;
    IOWorkLoop* fWorkLoop = nullptr;   // likely already present
    
    void vblankTick(IOTimerEventSource* sender);

    
 
    void*    getFB() const { return kernelFBPtr; }
    size_t   getFBSize() const { return kernelFBSize; }
    uint64_t getFBPhysAddr() const { return kernelFBPhys; }



    FakeIrisXEExeclist* getExeclist() const { return fExeclist; }
    FakeIrisXERing* getRcsRing() const { return fRcsRing; }
    
    
    
    IOBufferMemoryDescriptor* framebufferMemory;
    void* kernelFBPtr = nullptr;
    IOPhysicalAddress kernelFBPhys = 0;
    IOByteCount kernelFBSize = 0;
    IOMemoryMap* framebufferMap = nullptr;

    bool initGuCSystem();

    
    IOReturn performFlushNow();
    static IOReturn staticPerformFlush(OSObject *owner,
                                       void *arg0, void *arg1,
                                       void *arg2, void *arg3);

    
    bool makeUsable();
    
        static IOReturn staticStopAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
        void performSafeStop(); // actual cleanup executed on workloop/gated thread

    
    IOMemoryDescriptor* gttMemoryDescriptor;
      IOMemoryMap* gttMemoryMap;
      IOMemoryMap* ggttMemoryMap;
    

    uint32_t fbGGTTOffset = 0x00000800;

    
 virtual   IOReturn getNotificationSemaphore(IOSelect event,semaphore **sem)override;
    
   virtual IOReturn setCLUTWithEntries(IOColorEntry *entries, SInt32 index,
 SInt32 numEntries, IOOptionBits options);
  
    
    IOReturn setBackingStoreState(IODisplayModeID mode, IOOptionBits options);
    
    IOReturn setStartupDisplayMode(IODisplayModeID mode, IOIndex depth)override;
    
    
    
   virtual IOReturn newUserClient(task_t owningTask,
                                                  void* securityID,
                                                  UInt32 type,
                                                  OSDictionary* properties,
                                                  IOUserClient **handler)override;
    
    
    IOReturn waitForAcknowledge(IOIndex connect, UInt32 type, void *info);
    
    
    bool gpuPowerOn();

    
    bool waitForExeclistEvent(uint32_t timeoutMs);
    void* fSleepToken = (void*)0x12345678; // any unique pointer
    FakeIrisXERing* fRcsRing;   // render ring
    FakeIrisXEGEM* createTinyBatchGem();

    
    // GGTT mapping area (aperture)
    volatile uint32_t* fGGTT = nullptr;
    uint64_t fGGTTSize = 0;       // bytes
    uint64_t fGGTTBaseGPU = 0;    // start VA

    // Simple bump allocator state
    uint64_t fNextGGTTOffset = 0;

    
 
    uint64_t ggttMap(FakeIrisXEGEM* gem);
    void ggttUnmap(uint64_t gpuAddr, uint32_t pages);

    // ===========================
    // RCS Ring + GGTT + BAR0
    // ===========================
    volatile uint32_t* fBar0 = nullptr;       // MMIO BAR0 virtual mapping
    FakeIrisXERing*    fRingRCS = nullptr;    // Render Command Streamer ring

    uint64_t fGGTTBase = 0;                   // Base GGTT physical address
    uint32_t fGTTMMIOOffset = 0;              // from config space

    // Temporary batch GEM for testing
    FakeIrisXEGEM* batchGem = nullptr;

    FakeIrisXEGEM*    fFenceGEM = nullptr;
    uint64_t          fRingGpuVA = 0;             // GPU VA of ring buffer (GGTT)
    size_t            fRingSize = 0;              // bytes
    uint32_t fFenceSeq;
    FakeIrisXEGEM* fRingGem = nullptr;  // <--- Add this

    
    

    
    FakeIrisXERing* createRcsRing(size_t bytes);

    uint32_t submitBatch(FakeIrisXEGEM* batchGem, size_t batchOffsetBytes, size_t batchSizeBytes);
    
    
    
    
    uint32_t appendFenceAndSubmit(FakeIrisXEGEM* userBatchGem, size_t userBatchOffsetBytes, size_t userBatchSizeBytes);
   
    void handleInterrupt(IOInterruptEventSource* src, int count);
    bool addPendingSubmission(uint32_t seq, FakeIrisXEGEM* master, FakeIrisXEGEM* tail);
    bool completePendingSubmission(uint32_t seq);
    void cleanupAllPendingSubmissions();

    bool setBacklightPercent(uint32_t percent);
    
    uint32_t getBacklightPercent();

    void initBacklightHardware();

    FakeIrisXEGEM* createSimpleUserBatch();
    
    void dumpIRQAndRingRegsSafe();
    
    void enableRcsInterruptsSafely();

    
        bool forcewakeRenderHold(uint32_t timeoutMs = 2000);   // request & wait for FW ack
        void forcewakeRenderRelease();                         // drop FW
        void ensureEngineInterrupts();                         // minimal IER for engine
   
    FakeIrisXEExeclist* fExeclist = nullptr;

    
   
protected:
   
    void* gttVa = nullptr;
     IOVirtualAddress gttVA = 0;
     volatile uint64_t* ggttMMIO = nullptr;
    IOBufferMemoryDescriptor* textureMemory;
    size_t textureMemorySize;

    
    
    
    static void handleInterruptTrampoline(OSObject *owner, IOInterruptEventSource *src, int count) {
        FakeIrisXEFramebuffer *self = OSDynamicCast(FakeIrisXEFramebuffer, owner);
        if (!self) return;
        self->handleInterrupt(src, count);
    }

    
    

private:
    IOInterruptEventSource* fInterruptSource = nullptr;

    // simple struct to keep pending submissions if you want cleanup:
    struct Submission {
        uint32_t seq;
        FakeIrisXEGEM* masterGem;
        FakeIrisXEGEM* tailGem;
        // add timestamp, owner, etc.
    };
    OSArray* fPendingSubmissions = nullptr; // array of Submission objects or custom wrapper
    IOLock* fPendingLock = nullptr;

    IOCommandGate*  fCmdGate          = nullptr;
   
    FakeIrisXEBacklight* fBacklight = nullptr;
    
    
    
    
    // Firmware data storage
     
      bool fGuCEnabled;
      
      // GuC manager instance (if you create one)
      class FakeIrisXEGuC* fGuC;
    


};






/* #endif  _FAKE_IRIS_XE_FRAMEBUFFER_HPP_ */
