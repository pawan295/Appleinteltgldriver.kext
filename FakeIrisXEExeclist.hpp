//
//  FakeIrisXEExeclist.hpp
//  FakeIrisXEFramebuffer
//
//  Created by Anomy on 01/12/25.
//

//
//  FakeIrisXEExeclist.hpp
//  FakeIrisXEFramebuffer
//
//  Phase 7 – Execlist Support
//

#ifndef FakeIrisXEExeclist_hpp
#define FakeIrisXEExeclist_hpp

#include <IOKit/IOLib.h>
#include "FakeIrisXEGEM.hpp"
#include "FakeIrisXERing.h"

// Forward declaration
class FakeIrisXEFramebuffer;
class FakeIrisXEExeclist : public OSObject {
    OSDeclareDefaultStructors(FakeIrisXEExeclist)

public:
    static FakeIrisXEExeclist* withOwner(FakeIrisXEFramebuffer* owner);
    void free()override;

    bool createHwContext();
    void freeHwContext();
    bool setupExeclistPorts();
    bool submitBatchExeclist(FakeIrisXEGEM* batchGem);
    FakeIrisXEGEM* createRealBatchBuffer(const uint8_t* data, size_t len);

     bool submitBatchWithExeclist(FakeIrisXEFramebuffer* fb,
                                        FakeIrisXEGEM* batchGem,
                                        size_t batchSize,
                                        FakeIrisXERing* ring,
                                        uint32_t timeoutMs = 2000);

    bool programRcsForContext(FakeIrisXEFramebuffer* fb, uint64_t ctxGpuAddr, FakeIrisXEGEM* ringGem, uint64_t ringGpuAddr);

    #define MAX_EXECLIST_QUEUE 4

    struct XEHWContext {
        uint32_t        ctxId;          // software/Metal ctx id
        uint32_t        priority;       // higher = more important
        uint32_t        banScore;       // accumulated faults
        bool            banned;

        FakeIrisXEGEM*  lrcGem;
        uint64_t        lrcGGTT;

        FakeIrisXEGEM*  ringGem;
        uint64_t        ringGGTT;

        FakeIrisXEGEM*  fenceGem;
        uint64_t        fenceGGTT;
    };

        void handleCSB(); // called from interrupt
    

    struct ExecQueueEntry {
        XEHWContext*    hwCtx;
        FakeIrisXEGEM*  batchGem;
        uint64_t        batchGGTT;
        uint32_t        seqno;          // submission sequence

        // software state flags:
        bool            inFlight;
        bool            completed;
        bool            faulted;
    };

    static const uint32_t kMaxExeclistQueue  = 16;
    static const uint32_t kMaxHwContexts     = 16;
    static const uint32_t kMaxBanScore       = 3;

    
    public:
        FakeIrisXEFramebuffer* fOwner;

        // Global engine context list
        XEHWContext            fHwContexts[kMaxHwContexts];
        uint32_t               fHwContextCount;

        // Software execlist queue
        ExecQueueEntry         fQueue[kMaxExeclistQueue];
        uint32_t               fQHead;
        uint32_t               fQTail;
        uint32_t               fNextSeqno;

        // Currently running contexts (two ELSP slots)
        XEHWContext*           fInflight[2];
        uint32_t               fInflightSeqno[2];

        // CSB state
        FakeIrisXEGEM*         fCsbGem;
        uint64_t               fCsbGGTT;
        uint32_t               fCsbSizeBytes;
        uint32_t               fCsbEntryCount;
        uint32_t               fCsbReadIndex;   // software head

        // Single global LRC context from earlier can remain for simple paths,
        // but for multi-context execlists we mostly use XEHWContext entries.

        // ---- API ----

        // New: register HW context per ctxId (from Accelerator)
        XEHWContext* createHwContextFor(uint32_t ctxId, uint32_t priority);
        XEHWContext* lookupHwContext(uint32_t ctxId);

        // New: main submit entry point
        bool submitForContext(XEHWContext* hw, FakeIrisXEGEM* batchGem);

        // Low-level ELSP writer (single slot)
        bool submitToELSPSlot(int slot, ExecQueueEntry* e);

        // Called from framebuffer IRQ
        void engineIrq(uint32_t iir);

        // CSB handling
        void processCsbEntries();
        void handleCsbEntry(uint64_t low, uint64_t high);

        // React to events
        void onContextComplete(uint32_t ctxId, uint32_t status);
        void onContextFault(uint32_t ctxId, uint32_t status);

        // Scheduling helpers
        ExecQueueEntry* pickNextReady();
        void maybeKickScheduler();

        // Existing helpers
        uint32_t mmioRead32(uint32_t off);
        void     mmioWrite32(uint32_t off, uint32_t val);

        // (Your previous programRcsForContext / submitBatchWithExeclist-test
        //  can stay but will be gradually replaced by submitForContext())
    

    
    
    
    
    
private:

    uint64_t getCSBGpuAddress() const { return fCsbGGTT; }

    
    
    FakeIrisXEGEM* fLrcGem = nullptr;
    uint64_t       fLrcGGTT = 0;

     bool writeExeclistDescriptor(FakeIrisXEFramebuffer* fb, uint64_t ctxGpuAddr, uint64_t batchGpuAddr, size_t batchSize);

    static void write_le32(uint8_t* p, uint32_t v) { *(uint32_t*)p = v; }
    static void write_le64(uint8_t* p, uint64_t v) { *(uint64_t*)p = v; }









};

#endif
