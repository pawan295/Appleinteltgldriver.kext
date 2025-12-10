#ifndef FAKE_IRIS_XE_ACCELERATOR_HPP
#define FAKE_IRIS_XE_ACCELERATOR_HPP

#include <IOKit/IOService.h>
#include <FakeIrisXEAcceleratorUserClient.hpp>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOWorkLoop.h>

#include "FakeIrisXEGEM.hpp"
#include "FakeIrisXERing.h"
#include "i915_reg.h"

#include "FakeIrisXEExeclist.hpp"




// Forward-declare the framebuffer class
class FakeIrisXEFramebuffer;

// Include the shared structures used in public methods
#include "FakeIrisXEAccelShared.h"

class FakeIrisXEAccelerator : public IOService {
    OSDeclareDefaultStructors(FakeIrisXEAccelerator)

public:
    
    typedef IOService super;

    
    /**
     * @struct XEContext
     * @brief Stores per-context state, including its bound surface.
     * This structure is stored inside an OSData wrapper within the fContexts array.
     */
    struct XEContext {
        uint32_t ctxId{0};
        bool     active{false};
        uint64_t sharedGPUPtr{0}; // Shared data pointer from client

        // Surface data
        bool     hasSurface{false};
        uint32_t surfWidth{0};
        uint32_t surfHeight{0};
        uint32_t surfRowBytes{0};
        uint32_t surfPixelFormat{0};
        uint32_t surfIOSurfaceID{0};
        uint32_t surfID{0};
        void* surfCPU{nullptr}; // user-space mapped CPU pointer
    };

    // --- IOService Overrides ---
    bool init(OSDictionary *dictionary = nullptr) override;
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;

    // --- Public API (Called by UserClient) ---

    /**
     * @brief Attaches the shared memory ring buffer from the user client.
     * @param page The IOBufferMemoryDescriptor for the shared memory.
     * @return true on success.
     */
    bool attachShared(IOBufferMemoryDescriptor* page);

    /**
     * @brief Creates a new accelerator context.
     * @param sharedPtr Client-space pointer to shared data.
     * @param flags Creation flags.
     * @return A non-zero context ID on success, 0 on failure.
     */
    uint32_t createContext(uint64_t sharedPtr, uint32_t flags);

    /**
     * @brief Destroys an accelerator context.
     * @param ctxId The ID of the context to destroy.
     * @return kIOReturnSuccess.
     */
    bool destroyContext(uint32_t ctxId);

    /**
     * @brief Binds a surface's metadata to a context.
     * @param ctxId The context ID.
     * @param in Input parameters (size, format, CPU pointer, etc.).
     * @param out Output parameters (e.g., status).
     * @return kIOReturnSuccess on success, or an error code.
     */
    IOReturn bindSurface(uint32_t ctxId, const XEBindSurfaceIn& in, XEBindSurfaceOut& out);

    
    // Ensure these are declared in the public section of the class
    void startWorkerLoop();                                          // start worker timer/workloop (idempotent)
    void getCaps(XEAccelCaps& out);                                  // fill caps struct
    IOReturn flush(uint32_t ctxId = 0);                              // flush (called by UC)
    IOReturn bindSurfaceToContext(uint32_t ctxId, uint32_t surfID);  // bind IOSurface to ctx
    IOBufferMemoryDescriptor* getSharedMD() const { return fSharedMem; } // expose shared MD if needed

    
    // Shared Ring Buffer
    IOBufferMemoryDescriptor* fSharedMem {nullptr};
    volatile XEHdr* fHdr       {nullptr};
    uint8_t* fRingBase  {nullptr}; // Points after the XEHdr

    
    
    static void timerCallback(OSObject* owner, IOTimerEventSource* sender);

   
    void pollRing(IOTimerEventSource* sender);

    // Create the timer and add it to the workloop. Return true on success.
    static bool createAndArmTimer(FakeIrisXEAccelerator* self, IOWorkLoop* wl, IOTimerEventSource*& timerOut, uint32_t ms)
    {
        if (!self || !wl) return false;

        // if already exists, reschedule
        if (timerOut) {
            IOLog("(FakeIrisXEFramebuffer) [Accel] createAndArmTimer: already exists\n");
            timerOut->setTimeoutMS(ms);
            return true;
        }

        // Create timer using member-function cast to pollRing
        timerOut = IOTimerEventSource::timerEventSource(
            self,
            OSMemberFunctionCast(IOTimerEventSource::Action, self, &FakeIrisXEAccelerator::pollRing)
        );

        if (!timerOut) {
            IOLog("(FakeIrisXEFramebuffer) [Accel] createAndArmTimer: timerEventSource() failed\n");
            return false;
        }

        kern_return_t rc = (wl->addEventSource(timerOut) == kIOReturnSuccess) ? KERN_SUCCESS : KERN_FAILURE;
        if (rc != KERN_SUCCESS) {
            IOLog("(FakeIrisXEFramebuffer) [Accel] createAndArmTimer: addEventSource() failed\n");
            timerOut->release();
            timerOut = nullptr;
            return false;
        }

        // Arm it
        timerOut->setTimeoutMS(ms);
        IOLog("(FakeIrisXEFramebuffer) [Accel] createAndArmTimer: created and armed %u ms\n", ms);
        return true;
    }
     
     
     

    // Workloop & Timer
    IOWorkLoop* fWL {nullptr};
    IOTimerEventSource* fTimer {nullptr};
    
    
    volatile bool fPollActive { false };
    volatile bool fNeedFlush { false };

    
    
    
  //  IOSurfaceRef fPresentSurf = nullptr;
    uint32_t fPresentX, fPresentY, fPresentW, fPresentH;
    uint32_t fPresentRow = 0;
    bool     fPresentInProgress = false;


    IOLock *contextsLock;    // protects contexts array
        OSArray *contexts;       // stores OSData wrappers containing XECtx*
        uint32_t nextCtxId;

        // helper methods
        uint32_t createContext();
        XECtx* findCtx(uint32_t ctxId);

        bool bindSurface_UserMapped(uint32_t ctxId, const void* userPtr, size_t bytes, uint32_t rowbytes, uint32_t w, uint32_t h);
        bool presentContext(uint32_t ctxId);


        // init/teardown helpers
        bool initContexts();
        void freeContexts();
    
    
    
    FakeIrisXEFramebuffer* fFramebuffer = nullptr;

    bool submitGpuBatchForCtx(uint32_t ctxId,
                                                     FakeIrisXEGEM* batchGem,
                                                     uint32_t priority);
    
    
    FakeIrisXEFramebuffer* getFramebufferOwner() { return fFB; }

    
    FakeIrisXEExeclist* fExeclistFromFB = nullptr;
    FakeIrisXERing*     fRcsRingFromFB  = nullptr;

    void linkFromFramebuffer(FakeIrisXEFramebuffer* fb);

    
private:
  
    void processCommand(const XECmd &cmd, const void* payload, uint32_t payloadBytes);

    // --- Context Management ---
    
    /**
     * @brief Finds a context by its ID.
     * @note Must be called with fCtxLock held.
     */
    XEContext* lookupContext(uint32_t ctxId);

    // --- 2D Primitive Operations ---
    
    /**
     * @brief Handles the XE_CMD_CLEAR command.
     */
    void cmdClear(uint32_t argb);
    
    /**
     * @brief Handles the XE_CMD_RECT command.
     */
    void cmdRect(const XERectPayload& p);
    
    /**
     * @brief Handles the XE_CMD_COPY command.
     */
    void cmdCopy(const XECopyPayload& p);

    // --- Member Variables ---

    // Framebuffer
    FakeIrisXEFramebuffer* fFB {nullptr};
    void* fPixels{nullptr};   // Kernel-mapped FB pointer
    uint32_t                  fW{0}, fH{0}, fStride{0};


    // Context Management
    OSArray* fContexts {nullptr};
    IOLock* fCtxLock {nullptr};
    uint32_t                  fNextCtxId {1};
    
private:
    FakeIrisXEExeclist* fExeclist = nullptr;
    FakeIrisXERing*     fRcsRing = nullptr;

    
    
    
};


#endif // FAKE_IRIS_XE_ACCELERATOR_HPP
