/* #ifndef _FAKE_IRIS_XE_FRAMEBUFFER_HPP_ */
/* #define _FAKE_IRIS_XE_FRAMEBUFFER_HPP_ */
#pragma once

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

extern "C" void OSMemoryBarrier(void);
#define OSMemoryBarrier() __asm__ volatile("" ::: "memory")

      
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
    
    /*
    IOBufferMemoryDescriptor* fFBMemoryDescriptor;
    void* fFB;
    */
    
    
    
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

    
    
    bool                   initializeHardware();
     bool                   setupVRAM();
     bool                   setupDisplayModes();
     
    IOIndex currentConnection;
      bool displayOnline;
    bool fullyInitialized = false;
    IOLock* powerLock;
    bool shuttingDown = false;

    // Safe MMIO access methods
       uint32_t safeMMIORead(uint32_t offset) {
           if (!mmioBase || offset >= mmioMap->getLength()) {
               IOLog("⚠️ Invalid MMIO read at 0x%X\n", offset);
               return 0xFFFFFFFF;
           }
           return *(volatile uint32_t*)(mmioBase + offset);
       }
       
       void safeMMIOWrite(uint32_t offset, uint32_t value) {
           if (!mmioBase || offset >= mmioMap->getLength()) {
               IOLog("⚠️ Invalid MMIO write at 0x%X\n", offset);
               return;
           }
           *(volatile uint32_t*)(mmioBase + offset) = value;
                  #ifdef OSMemoryBarrier
                  OSMemoryBarrier();
                  #else
                  __asm__ volatile("mfence" ::: "memory"); // x86 specific
                  #endif

       }
    
    
    
    
    
 public:
    
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

    
    IOBufferMemoryDescriptor* framebufferMemory;
    
    IOBufferMemoryDescriptor* fFramebufferMemory { nullptr };
 
    
     uint64_t getFramebufferPhysAddr() const {
         return framebufferMemory ? framebufferMemory->getPhysicalAddress() : 0;
     }
    

    
    
//   uint64_t fFBPhys{0};

    IOBufferMemoryDescriptor* getFBMemory() const { return framebufferMemory; }
   // uint64_t getFramebufferPhysAddr() const { return fFBPhys; }

    
    void*    getFramebufferKernelPtr() const; // from IOBufferMemoryDescriptor::getBytesNoCopy()

    
    
    
    bool start(IOService* provider) override;
    void stop(IOService* provider) override;
    virtual bool           init(OSDictionary* dict) override;
    bool   setupWorkLoop();


    virtual bool isConsoleDevice() const;
    virtual void           free() override;
    void notifyServer(IOSelect event);
    virtual void startIOFB();
    virtual IOWorkLoop* getWorkLoop() const override;

    bool controllerEnabled = false;

    bool displayPublished = false; // ✅ Member variable
    
    void disableController();
    
    void publishDisplay();
    
    
    enum {
        kPowerStateOff = 0,
        kPowerStateOn,
        kNumPowerStates
    };

    static IOPMPowerState powerStates[kNumPowerStates];


    
    virtual IOReturn setAbltFramebuffer(void * buffer) ;     // Front-buffer pointer from WS

    
    
    virtual IOService *probe(IOService *provider, SInt32 *score) override;

    
    bool getIsUsable() const;
    
    
    virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice) override;

    
    virtual const char* getPixelFormats() override;
    
    virtual UInt64 getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
    virtual IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
   
    virtual IOReturn flushFramebuffer(void) ;
 
    
    virtual UInt32 getConnectionCount() override;
    
    virtual IOReturn getStartupDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) override;
    virtual IOReturn getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t *value) override;
    virtual IODeviceMemory* getVRAMRange() override;
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
    
    
//    virtual void deliverFramebufferNotification(IOIndex index, UInt32 event, void* info);

    
    virtual IOReturn setNumberOfDisplays(UInt32 count) ;
    
    
    
    
    virtual IODeviceMemory * getApertureRange(IOPixelAperture aperture) override;

    
    
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
    
    virtual IOReturn setCLUTWithEntries(IOColorEntry* colors,
                                                            UInt32 firstIndex,
                                                            UInt32 numEntries,
                                        IOOptionBits options)override;
        
    virtual IOReturn setGammaTable(UInt32 channelCount,
                                                      UInt32 dataCount,
                                                      UInt32 dataWidth,
                                                      void* data)override;
        
    virtual IOReturn createAccelTask(mach_port_t* port);
    
    
    
    virtual IOReturn setOnline(bool online);
    
    virtual IOReturn getConnectionFlags(IOIndex connectIndex, UInt32* flags);
    
    virtual IOReturn notifyServer(IOSelect message, void* data, size_t dataSize);
    
    virtual  IOReturn getGammaTable(UInt32 channelCount,
                                                  UInt32* dataCount,
                                                  UInt32* dataWidth,
                                                         void** data);

    virtual IOReturn setMode(IODisplayModeID displayMode, IOOptionBits options, UInt32 depth);

    
    virtual IOReturn getOnlineState(IOIndex connectIndex, bool* online);
    
    virtual IOReturn setOnlineState(IOIndex connectIndex, bool online);
    
  //  void vsyncTimerFired(OSObject* owner, IOTimerEventSource* sender);
    
   // void vsyncOccurred(OSObject* owner, IOInterruptEventSource* src, int count);
    

    // Essential methods for IOFramebufferUserClient to function
    virtual IOReturn getAttributeForIndex(IOSelect attribute, UInt32 index, UInt32* value) ;
    virtual IOReturn setProperties(OSObject* properties) override;

    // Optional but recommended for display control
    virtual IOReturn validateDetailedTiming(void* desc, UInt32* score) ;
    virtual IOReturn setDetailedTimings(OSObject* params) ;
    virtual IOReturn setInterruptState(void* ref, UInt32 state) override;
    virtual IOReturn handleEvent(IOFramebuffer* fb, void* ref, UInt32 event, void* info) ;

    // Optional: supports brightness, gamma, transform, etc.
    virtual IOReturn doControl(UInt32 command, void* params, UInt32 size) ;
    virtual IOReturn extControl(OSObject* params) ;
    virtual void transformLocation(IOGPoint* loc, IOOptionBits options) ;
    
    // in class FakeIrisXEFramebuffer : public IOService / IOFramebuffer...
    IOTimerEventSource* fVBlankTimer = nullptr;
    IOWorkLoop* fWorkLoop = nullptr;   // likely already present
    
    void vblankTick(IOTimerEventSource* sender);

    
 
    void*    getFB() const { return kernelFBPtr; }
    size_t   getFBSize() const { return kernelFBSize; }
    uint64_t getFBPhysAddr() const { return kernelFBPhys; }

    void*    kernelFBPtr   = nullptr;
        size_t   kernelFBSize  = 0;
        uint64_t kernelFBPhys  = 0;
    
    
    
    IOReturn performFlushNow();
    static IOReturn staticPerformFlush(OSObject *owner,
                                       void *arg0, void *arg1,
                                       void *arg2, void *arg3);

    
    

    IOReturn newUserClient(task_t owningTask,
                                                  void* securityID,
                                                  UInt32 type,
                                                  OSDictionary* properties,
                                                  IOUserClient **handler)override;
    
    bool makeUsable();
    
        static IOReturn staticStopAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
        void performSafeStop(); // actual cleanup executed on workloop/gated thread

    
    IOMemoryDescriptor* gttMemoryDescriptor;
      IOMemoryMap* gttMemoryMap;
      IOMemoryMap* ggttMemoryMap;
    

    uint32_t fbGGTTOffset = 0x00000800;
    
    bool open(IOService* client, IOOptionBits opts);

    void close(IOService* client, IOOptionBits opts)override;

    IOReturn handleGetAttribute(
                                                       IOIndex connect, IOSelect attribute, uintptr_t* value);

    
    bool fIsOpen;

    virtual IOIndex getStartupDepth(void) ;
    
    
    
    
    
    
private:
   
    void* gttVa = nullptr;
     IOVirtualAddress gttVA = 0;
     volatile uint64_t* ggttMMIO = nullptr;
    


};

/* #endif  _FAKE_IRIS_XE_FRAMEBUFFER_HPP_ */
