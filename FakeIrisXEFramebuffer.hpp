#ifndef _FAKE_IRIS_XE_FRAMEBUFFER_HPP_
#define _FAKE_IRIS_XE_FRAMEBUFFER_HPP_

#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/pci/IOPCIDevice.h> // for IOPCIDevice
#include <IOKit/IOBufferMemoryDescriptor.h> // for IODeviceMemory
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/graphics/IODisplay.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

      
class FakeIrisXEFramebuffer : public IOFramebuffer

{
    OSDeclareDefaultStructors(FakeIrisXEFramebuffer)

    
private:
    
    IOInterruptEventSource* vsyncSource;
        IOTimerEventSource* vsyncTimer;

    
protected:
    IOMemoryMap* vramMap;
    IOMemoryMap* mmioMap;
    IOBufferMemoryDescriptor* vramMemory;
        IOPCIDevice* pciDevice;
        volatile UInt8* mmioBase;
        IODisplayModeID currentMode;
        IOIndex currentDepth;
        size_t vramSize;
    IOWorkLoop* workLoop;
    IOCommandGate* commandGate;
    IOBufferMemoryDescriptor* framebufferSurface;
    IOBufferMemoryDescriptor* cursorMemory;
    IOBufferMemoryDescriptor* framebufferMemory;
    
    bool                   initializeHardware();
     bool                   setupVRAM();
     bool                   setupDisplayModes();
     
    IOIndex currentConnection;
      bool displayOnline;
    
    
    
    
    
 public:
    bool start(IOService* provider) override;
void stop(IOService* provider) override;
    virtual bool isConsoleDevice()override;
    virtual bool           init(OSDictionary* dict) override;
    virtual void           free() override;
    void notifyServer(IOSelect event);
    virtual void startIOFB();
    
    enum {
        kPowerStateOff = 0,
        kPowerStateOn = 1
    };

    static IOPMPowerState powerStates[2];
    
    
    
    virtual IOService *probe(IOService *provider, SInt32 *score) override;

    
    virtual IOReturn setPowerState(unsigned long whichState, IOService* whatDevice) override;

    
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

    IOReturn enableController() override;


    IOReturn getPixelInformation(IODisplayModeID displayMode,
                                 IOIndex depth,
                                 IOPixelAperture aperture,
                                 IOPixelInformation* info) override;

    IOReturn getCurrentDisplayMode(IODisplayModeID* displayMode, IOIndex* depth) override;

    

    virtual IOReturn setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t value) override;
    
    virtual IOReturn flushDisplay(void) ;
        virtual void deliverFramebufferNotification(IOIndex index, UInt32 event, void* info) ;
        virtual IOReturn setNumberOfDisplays(UInt32 count) ;
    
    
    
    
    virtual IODeviceMemory* getApertureRange(IOPixelAperture aperture) override;
    
    virtual IOIndex getAperture() const ;

    
    
    virtual IOItemCount getDisplayModeCount(void) override;

    
        virtual IOReturn  getAttribute(IOSelect attribute, uintptr_t* value) override;
    
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
    
    
    virtual IOReturn notifyServer(IOSelect message, void* data, size_t dataSize);
    
   virtual IOReturn getVRAMRange(IOPhysicalAddress* start,
                                                 IOPhysicalLength* length);
        
    virtual  IOReturn getGammaTable(UInt32 channelCount,
                                                  UInt32* dataCount,
                                                  UInt32* dataWidth,
                                                         void** data);
    
   virtual IOReturn getFramebufferMemory(void* memory) {
        return kIOReturnUnsupported;
    }

    virtual IOReturn setMode(IODisplayModeID displayMode, IOOptionBits options, UInt32 depth);

    
    virtual IOReturn getOnlineState(IOIndex connectIndex, bool* online);
    
    virtual IOReturn setOnlineState(IOIndex connectIndex, bool online);
    
    void vsyncTimerFired(OSObject* owner, IOTimerEventSource* sender);
    
    void vsyncOccurred(OSObject* owner, IOInterruptEventSource* src, int count);
 
    virtual IOReturn newUserClient(task_t owningTask,
                                                   void* securityID,
                                                   UInt32 type,
                                                  IOUserClient** handler)override;
    
    
    
    
    
};






#endif /* _FAKE_IRIS_XE_FRAMEBUFFER_HPP_ */
