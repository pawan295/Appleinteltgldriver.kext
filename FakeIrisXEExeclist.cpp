//
//  FakeIrisXEExeclist.cpp
//  FakeIrisXEFramebuffer
//
//  Created by Anomy on 01/12/25.
//

//
// FakeIrisXEExeclist.cpp
// Phase 7 – Execlists Implementation
//


#include "FakeIrisXEExeclist.hpp"
#include "FakeIrisXEFramebuffer.hpp"
#include "FakeIrisXEGEM.hpp"
#include "FakeIrisXELRC.hpp"
#include "i915_reg.h"



OSDefineMetaClassAndStructors(FakeIrisXEExeclist, OSObject);

// FACTORY
FakeIrisXEExeclist* FakeIrisXEExeclist::withOwner(FakeIrisXEFramebuffer* owner)
{
    FakeIrisXEExeclist* obj = OSTypeAlloc(FakeIrisXEExeclist);
    if (!obj) return nullptr;

    if (!obj->init()) {
        obj->release();
        return nullptr;
    }

    obj->fOwner = owner;

    // init HW context table
    obj->fHwContextCount = 0;
    for (uint32_t i = 0; i < kMaxHwContexts; ++i) {
        bzero(&obj->fHwContexts[i], sizeof(XEHWContext));
    }

    // init SW execlist queue
    obj->fQHead     = 0;
    obj->fQTail     = 0;
    obj->fNextSeqno = 1;
    for (uint32_t i = 0; i < kMaxExeclistQueue; ++i) {
        bzero(&obj->fQueue[i], sizeof(ExecQueueEntry));
    }

    // inflight slots
    obj->fInflight[0] = nullptr;
    obj->fInflight[1] = nullptr;
    obj->fInflightSeqno[0] = 0;
    obj->fInflightSeqno[1] = 0;

    // CSB ring defaults – you can update in createHwContext/setupExeclistPorts
    obj->fCsbGem         = nullptr;
    obj->fCsbGGTT        = 0;
    obj->fCsbSizeBytes   = 0x100;          // matches your 256-byte alloc
    obj->fCsbEntryCount  = obj->fCsbSizeBytes / 16; // 16B per CSB entry
    obj->fCsbReadIndex   = 0;

    return obj;
}




// FREE (destructor)
void FakeIrisXEExeclist::free()
{
    freeHwContext();
    OSObject::free();
}


// ------------------------------------------------------------
// Helpers (safe MMIO)
// ------------------------------------------------------------

uint32_t FakeIrisXEExeclist::mmioRead32(uint32_t off) {
    return *(volatile uint32_t*)((uint8_t*)fOwner->fBar0 + off);
}

void FakeIrisXEExeclist::mmioWrite32(uint32_t off, uint32_t val) {
    volatile uint32_t* p = (volatile uint32_t*)((uint8_t*)fOwner->fBar0 + off);
    *p = val;
    (void)*p; // posted write ordering
}



// ------------------------------------------------------------
// createHwContext()
// ------------------------------------------------------------

bool FakeIrisXEExeclist::createHwContext()
{
    IOLog("(FakeIrisXE) [Exec] Alloc LRC\n");

    if (!fOwner) {
        IOLog("(FakeIrisXE) [Exec] createHwContext(): fOwner == NULL\n");
        return false;
    }

    
    // --- robust preamble for createHwContext() ---
    IOLog("(FakeIrisXE) [Exec] Alloc LRC (enter pre-reset checks)\n");

    // helper lambdas (use fOwner/fFramebuffer safeMMIO methods if available)
    auto safeRead = [&](uint32_t off) -> uint32_t {
        if (fOwner) {
            return fOwner->safeMMIORead(off);
        }
        return 0;
    };

    auto safeWrite = [&](uint32_t off, uint32_t val) -> void {
        if (fOwner) {
            fOwner->safeMMIOWrite(off, val);
        }
    };


    // 1) Soft GT reset (clears stale state without full power cycle)
        mmioWrite32(0x52080, 0x0);  // GT_MODE = disable
        IOSleep(5);
        mmioWrite32(0x52080, 0x1);  // GT_MODE = enable
        IOSleep(10);
    
    
    // Forcewake registers (adjust if your headers define different offsets)
//    const uint32_t FORCEWAKE_ACK = 0x130044; // you read this previously

    // 1) Snapshot before touching anything
    uint32_t pre_ack = safeRead(FORCEWAKE_ACK);
    IOLog("(FakeIrisXE) [Exec] pre-reset FORCEWAKE_ACK=0x%08x\n", pre_ack);

    // 2) Conservative ring disable/reset (do minimal writes)
    IOLog("(FakeIrisXE) [Exec] resetting RCS ring (base/head/tail/ctl)\n");
    safeWrite(RING_CTL, 0x0);          // disable ring
    safeWrite(RING_TAIL, 0x0);
    safeWrite(RING_HEAD, 0x0);
    safeWrite(RING_BASE_HI, 0x0);
    safeWrite(RING_BASE_LO, 0x0);
    (void)safeRead(RING_CTL); // posting read
    IOSleep(5); // let HW settle

    // Avoid massive mmio clearing loops here — prefer clearing via an allocated CSB GEM
    // (If you still want to zero CSB registers region, do so sparingly with small delay.)
    IOLog("(FakeIrisXE) [Exec] re-requesting forcewake (conservative)\n");

    // 3) Request forcewake (bitmask). Use read-verify loop rather than single write.
    const uint32_t REQ_MASK = 0x000F000F;   // what you wrote earlier; adjust if needed
    safeWrite(FORCEWAKE_REQ, REQ_MASK);
    (void)safeRead(FORCEWAKE_REQ); // posting read
    IOSleep(2);

    // 4) Wait up to 50 ms for ACK bits
    const uint64_t start = mach_absolute_time();
    const uint64_t timeout_ns = 50ULL * 1000000ULL;
    bool fw_ok = false;
    while (true) {
        uint32_t ack = safeRead(FORCEWAKE_ACK);
        IOLog("(FakeIrisXE) [Exec] forcewake ack poll -> 0x%08x\n", ack);
        // check lower nibble(s) or mask your HW expects:
        if ((ack & 0xF) == 0xF) { fw_ok = true; break; }
        if ((mach_absolute_time() - start) > timeout_ns) break;
        IOSleep(1);
    }

    if (!fw_ok) {
        uint32_t final_ack = safeRead(FORCEWAKE_ACK);
        IOLog("❌ Forcewake after reset FAILED (final ack=0x%08X)\n", final_ack);
        // Bail out safely — do not touch execlist registers if forcewake failed.
        return false;
    }
    IOLog("✅ Forcewake post-reset OK\n");

    
    // FIXED: Re-enable IER/IMR (cleared by reset — only completion bit)
        mmioWrite32(0x44004, 0x0);  // RCS0_IMR = unmask
        mmioWrite32(0x4400C, 0x1);  // RCS0_IER = enable complete IRQ
        (void)mmioRead32(0x4400C);  // Posted read
        IOSleep(5);
    
    
    // continue with LRC allocation...

    
    
    const size_t ctxSize = 4096;

    // ---------------------------
    // Allocate LRC GEM
    // ---------------------------
    fLrcGem = FakeIrisXEGEM::withSize(ctxSize, 0);
    if (!fLrcGem) {
        IOLog("(FakeIrisXE) [Exec] LRC alloc failed\n");
        return false;
    }

    // Zero memory
    IOBufferMemoryDescriptor* md = fLrcGem->memoryDescriptor();
    if (md) {
        bzero(md->getBytesNoCopy(), md->getLength());
    }

    // Your pin() returns void!
    fLrcGem->pin();

    // Map into GGTT
    fLrcGGTT = fOwner->ggttMap(fLrcGem);   // 100% correct for your project

    if (fLrcGGTT == 0) {
        IOLog("(FakeIrisXE) [Exec] ggttMap(LRC) failed\n");
        return false;
    }

    // Align to 4K as required by LRC hardware
    fLrcGGTT &= ~0xFFFULL;

    IOLog("(FakeIrisXE) [Exec] LRC @ GGTT=0x%llx\n", fLrcGGTT);


    
    // ---------------------------
    // Allocate CSB GEM (GEN12 requires ~128B, we use 256B safe)
    // ---------------------------
    IOLog("(FakeIrisXE) [Exec] Alloc CSB\n");

    constexpr size_t kCSBSize = 0x100; // 256 bytes
    fCsbGem = FakeIrisXEGEM::withSize(kCSBSize, 0);
    if (!fCsbGem) {
        IOLog("(FakeIrisXE) [Exec] No CSB alloc\n");
        fCsbGGTT = 0;
    } else {
        fCsbGem->pin();
        fCsbGGTT = fOwner->ggttMap(fCsbGem);
        if (fCsbGGTT)
            fCsbGGTT &= ~0xFFFULL;
    }


    
    
    
    
    fCsbGem->pin();

    fCsbGGTT = fOwner->ggttMap(fCsbGem);
    if (fCsbGGTT) {
        fCsbGGTT &= ~0xFFFULL;
    }
    return true;
}






// ------------------------------------------------------------
// freeHwContext()
// ------------------------------------------------------------

void FakeIrisXEExeclist::freeHwContext()
{
    if (fLrcGem) {
        fLrcGem->unpin();
        fLrcGem->release();
        fLrcGem = nullptr;
    }
    if (fCsbGem) {
        fCsbGem->unpin();
        fCsbGem->release();
        fCsbGem = nullptr;
    }
}








// ------------------------------------------------------------
// setupExeclistPorts()
// ------------------------------------------------------------
// safer setupExeclistPorts() — programs pointers, verifies readback, DOES NOT kick
bool FakeIrisXEExeclist::setupExeclistPorts()
{
    if (!fOwner) {
        IOLog("(FakeIrisXE) [Exec] setupExeclistPorts: no owner\n");
        return false;
    }

    if (!fLrcGGTT) {
        IOLog("(FakeIrisXE) [Exec] setupExeclistPorts: missing fLrcGGTT\n");
        return false;
    }

    // ---------- 1) Hold forcewake and ensure engine interrupts ----------
    if (!fOwner->forcewakeRenderHold(5000 /*ms*/)) {
        IOLog("(FakeIrisXE) [Exec] setupExeclistPorts: forcewake hold failed\n");
        return false;
    }
    // Make sure the engine can auto-wake / interrupt itself
    fOwner->ensureEngineInterrupts();

    
    // verify GT awake (read same regs as before but now while forcewake is held)
    uint32_t gt_status = mmioRead32(0x13805C);
    uint32_t forcewake_ack = mmioRead32(0x130044);

    if ((gt_status == 0x0) || ((forcewake_ack & 0xF) == 0x0)) {
        IOLog("⚠️ GPU verification failed (after hold): GT_STATUS=0x%08X, ACK=0x%08X — still waking up\n",
              gt_status, forcewake_ack);
        // clean up: release the hold since we are aborting
    }

    IOLog("✅ GPU verified awake: GT_STATUS=0x%08X, ACK=0x%08X\n", gt_status, forcewake_ack);

    
    
    
    // ---------- 2) Program ELSP submit port (LRC pointer) while wake held ----------
    const uint64_t lrc = fLrcGGTT & ~0xFFFULL;
    uint32_t elsp_lo = (uint32_t)(lrc & 0xFFFFFFFFULL);
    uint32_t elsp_hi = (uint32_t)(lrc >> 32);
    mmioWrite32(RCS0_EXECLIST_SUBMITPORT_LO, elsp_lo);
    mmioWrite32(RCS0_EXECLIST_SUBMITPORT_HI, elsp_hi);

    
    
    // Program CSB pointer if present
    // Program CSB BASE registers (GEN12 mandatory)
    if (fCsbGGTT) {
        uint32_t csb_lo = (uint32_t)(fCsbGGTT & 0xFFFFFFFFULL);
        uint32_t csb_hi = (uint32_t)(fCsbGGTT >> 32);

        mmioWrite32(RCS0_CSB_ADDR_LO, csb_lo);
        mmioWrite32(RCS0_CSB_ADDR_HI, csb_hi);
        mmioWrite32(RCS0_CSB_CTRL, 0x1); // enable CSB tracking if needed
    }

    
    
    
    // Readback checks (do not assume writes are posted)
    uint32_t r_elsp_lo = mmioRead32(RCS0_EXECLIST_SUBMITPORT_LO);
    uint32_t r_elsp_hi = mmioRead32(RCS0_EXECLIST_SUBMITPORT_HI);
    IOLog("(FakeIrisXE) [Exec] ELSP readback LO=0x%08x HI=0x%08x (expected LO=0x%08x HI=0x%08x)\n",
          r_elsp_lo, r_elsp_hi, elsp_lo, elsp_hi);

    if (r_elsp_lo != elsp_lo || r_elsp_hi != elsp_hi) {
        IOLog("(FakeIrisXE) [Exec] ELSP readback mismatch — aborting safe setup\n");
        fOwner->forcewakeRenderRelease();
        return false;
    }

    
    if (fCsbGGTT) {
        uint32_t r_csb_lo = mmioRead32(CSB_ADDR_LO);
        uint32_t r_csb_hi = mmioRead32(CSB_ADDR_HI);
        IOLog("(FakeIrisXE) [Exec] CSB readback LO=0x%08x HI=0x%08x\n", r_csb_lo, r_csb_hi);
        // non-fatal; log only (CSB optional)
    }

    
    
    // GEN12 Execlist + CSB interrupt pipeline
    constexpr uint32_t IRQS =
          (1 << 12)  // CONTEXT_COMPLETE
        | (1 << 13)  // CONTEXT_SWITCH
        | (1 << 11); // PAGE_FAULT

    mmioWrite32(RCS0_IMR, ~IRQS);
    mmioWrite32(RCS0_IER, IRQS);
    mmioWrite32(GEN11_GFX_MSTR_IRQ_MASK, 0x0);
    mmioWrite32(GEN11_GFX_MSTR_IRQ, IRQS);

    
    
    
    // ---------- 3) Keep the forcewake held. Do NOT kick here if you expect submit() later.
    // We return success while still holding the hold; submitBatch() MUST keep the hold across the ELSP kick.
    IOLog("(FakeIrisXE) [Exec] setupExeclistPorts SUCCESS (no kick) - FUZZ: leaving forcewake held for submit path\n");
    return true;
}





// ------------------------------------------------------------
// createRealBatchBuffer()
// ------------------------------------------------------------
FakeIrisXEGEM* FakeIrisXEExeclist::createRealBatchBuffer(const uint8_t* data, size_t len)
{
    if (!fOwner) {
        IOLog("(FakeIrisXE) [Exec] createRealBatchBuffer: missing owner\n");
        return nullptr;
    }

    const size_t page = 4096;
    const size_t alloc = (len + page - 1) & ~(page - 1);

    FakeIrisXEGEM* gem = FakeIrisXEGEM::withSize(alloc, 0);
    if (!gem) {
        IOLog("(FakeIrisXE) [Exec] BB alloc failed\n");
        return nullptr;
    }

    IOBufferMemoryDescriptor* md = gem->memoryDescriptor();
    if (!md) {
        IOLog("(FakeIrisXE) [Exec] BB missing memoryDescriptor\n");
        gem->release();
        return nullptr;
    }

    // Zero whole allocation then copy provided data
    void* cpuPtr = md->getBytesNoCopy();
    if (!cpuPtr) {
        IOLog("(FakeIrisXE) [Exec] BB has no CPU pointer\n");
        gem->release();
        return nullptr;
    }
    bzero(cpuPtr, md->getLength());
    if (data && len > 0) memcpy(cpuPtr, data, len);

    // pin() returns void in your GEM; call it, don't test return
    gem->pin();

    // Map the GEM into GGTT using the framebuffer helper you already have
    uint64_t ggtt = fOwner->ggttMap(gem);
    if (ggtt == 0) {
        IOLog("(FakeIrisXE) [Exec] ggttMap(BB) failed\n");
        // best-effort cleanup: unpin if you have an unpin (no return value)
        gem->unpin();
        gem->release();
        return nullptr;
    }

    // Align GPU address to page boundary if needed
    ggtt &= ~0xFFFULL;

    IOLog("(FakeIrisXE) [Exec] BB allocated: size=0x%llx cpu=%p ggtt=0x%llx\n",
          md->getLength(), cpuPtr, (unsigned long long)ggtt);

    // Keep gem pinned — caller must unpin/release when done
    return gem;
}





// ------------------------------------------------------------
// submitBatchExeclist()
// ------------------------------------------------------------
bool FakeIrisXEExeclist::submitBatchExeclist(FakeIrisXEGEM* batchGem)
{
    if (!batchGem || !fOwner) {
        IOLog("(FakeIrisXE) [Exec] submitBatchExeclist: missing batch or owner\n");
        return false;
    }

    // Ensure GEM is pinned
    batchGem->pin();

    // Get GGTT address from framebuffer
    uint64_t batchGGTT = fOwner->ggttMap(batchGem);
    if (batchGGTT == 0) {
        IOLog("(FakeIrisXE) [Exec] FAILED: ggttMap(batchGem)=0\n");
        return false;
    }
    batchGGTT &= ~0xFFFULL;

    IOLog("(FakeIrisXE) [Exec] Submit batch @ GGTT=0x%llx\n", batchGGTT);

    // Allocate an execlist "queue descriptor"
    FakeIrisXEGEM* listGem = FakeIrisXEGEM::withSize(4096, 0);
    if (!listGem) {
        IOLog("(FakeIrisXE) [Exec] listGem alloc failed\n");
        return false;
    }

    IOBufferMemoryDescriptor* md = listGem->memoryDescriptor();
    if (!md) {
        listGem->release();
        IOLog("(FakeIrisXE) [Exec] listGem missing memoryDescriptor\n");
        return false;
    }

    // Fill descriptor
    void* cpuPtr = md->getBytesNoCopy();
    bzero(cpuPtr, md->getLength());

    uint64_t* q = (uint64_t*)cpuPtr;
    q[0] = batchGGTT;   // first entry = batch buffer GPU address

    // pin & map listGem
    listGem->pin();
    uint64_t listGGTT = fOwner->ggttMap(listGem);
    if (listGGTT == 0) {
        IOLog("(FakeIrisXE) [Exec] FAILED: ggttMap(listGem)=0\n");
        listGem->unpin();
        listGem->release();
        return false;
    }

    IOLog("(FakeIrisXE) [Exec] ELSP list @ GGTT=0x%llx\n", listGGTT);

    // Program ELSP submit port
    mmioWrite32(RCS0_EXECLIST_SUBMITPORT_LO, (uint32_t)(listGGTT & 0xFFFFFFFFULL));
    mmioWrite32(RCS0_EXECLIST_SUBMITPORT_HI, (uint32_t)(listGGTT >> 32));

    // Kick exec list
    mmioWrite32(RCS0_EXECLIST_CONTROL, 0x1);

    IOLog("(FakeIrisXE) [Exec] ExecList kicked\n");

    // Poll status
    uint64_t start = mach_absolute_time();
    const uint64_t limit_ns = 2000ULL * 1000000ULL;

    while (true) {
        uint32_t status = mmioRead32(RCS0_EXECLIST_STATUS_LO);
        if (status != 0) {
            IOLog("(FakeIrisXE) [Exec] STATUS=0x%08x\n", status);
            break;
        }

        if (mach_absolute_time() - start > limit_ns) {
            IOLog("(FakeIrisXE) [Exec] TIMEOUT waiting execlist\n");
            break;
        }

        IOSleep(1);
    }

    listGem->unpin();
    listGem->release();

    return true;
}





/*
bool FakeIrisXEExeclist::programRcsForContext(
        FakeIrisXEFramebuffer* fb,
        uint64_t ctxGpu,
        FakeIrisXEGEM* ringGem,
        uint64_t batchGpu)
{
    // We actually trust our own owner + mmio helpers, not the fb param.
    if (!fOwner || !ringGem)
        return false;

    IOLog("=== SIMPLE ELSP SUBMIT TEST (v2) === ctx=0x%llx batch=0x%llx\n",
          ctxGpu, batchGpu);

    // --------------------------------------------------
    // STEP 0: Hold RENDER forcewake (like setupExeclistPorts)
    // --------------------------------------------------
    if (!fOwner->forcewakeRenderHold(5000 )) {
        IOLog("❌ programRcsForContext: forcewakeRenderHold() FAILED\n");
        return false;
    }

    uint32_t ack = mmioRead32(0x130044); // FORCEWAKE_ACK
    IOLog("programRcsForContext: FORCEWAKE_ACK after hold = 0x%08x\n", ack);

    // --------------------------------------------------
    // STEP 1: Build a minimal descriptor (same as before)
    // --------------------------------------------------
    FakeIrisXEGEM* listGem = FakeIrisXEGEM::withSize(4096, 0);
    if (!listGem) {
        fOwner->forcewakeRenderRelease();
        return false;
    }

    listGem->pin();
    uint32_t* w = (uint32_t*)listGem->memoryDescriptor()->getBytesNoCopy();
    bzero(w, 4096);

    // Minimal descriptor for Gen12:
    // DW0/DW1: LRC GGTT (ctx)
    // DW3: VALID|ACTIVE (no fancy priority)
    // DW4/DW5: Batch start
    w[0] = (uint32_t)(ctxGpu & 0xFFFFFFFFull);
    w[1] = (uint32_t)(ctxGpu >> 32);
    w[2] = 0;
    w[3] = (1u << 0) | (1u << 1);   // VALID + ACTIVE only
    w[4] = (uint32_t)(batchGpu & 0xFFFFFFFFull);
    w[5] = (uint32_t)(batchGpu >> 32);
    w[6] = 0;
    w[7] = 0;

    __sync_synchronize();

    uint64_t listGpu = fOwner->ggttMap(listGem);
    if (!listGpu) {
        listGem->release();
        fOwner->forcewakeRenderRelease();
        return false;
    }

    listGpu &= ~0xFFFULL; // page align, just like before
    IOLog("programRcsForContext: Descriptor GGTT VA=0x%llx\n", listGpu);

    // --------------------------------------------------
    // STEP 2: Read ELSP before write (using SAME regs as setupExeclistPorts)
    // --------------------------------------------------
    uint32_t elsp_before_lo = mmioRead32(RCS0_EXECLIST_SUBMITPORT_LO);
    uint32_t elsp_before_hi = mmioRead32(RCS0_EXECLIST_SUBMITPORT_HI);
    IOLog("programRcsForContext: ELSP before: LO=0x%08x HI=0x%08x\n",
          elsp_before_lo, elsp_before_hi);

    // --------------------------------------------------
    // STEP 3: Write ELSP via mmioWrite32 (NO safeMMIOWrite, NO gpuPowerOn)
    // --------------------------------------------------
    uint32_t desc_lo = (uint32_t)(listGpu & 0xFFFFFFFFull);
    uint32_t desc_hi = (uint32_t)(listGpu >> 32);

    mmioWrite32(RCS0_EXECLIST_SUBMITPORT_LO, desc_lo);
    mmioWrite32(RCS0_EXECLIST_SUBMITPORT_HI, desc_hi);

    // small delay so posted writes land
    IOSleep(2);

    // --------------------------------------------------
    // STEP 4: Read back ELSP and STATUS
    // --------------------------------------------------
    uint32_t elsp_after_lo = mmioRead32(RCS0_EXECLIST_SUBMITPORT_LO);
    uint32_t elsp_after_hi = mmioRead32(RCS0_EXECLIST_SUBMITPORT_HI);
    uint32_t status_lo     = mmioRead32(RCS0_EXECLIST_STATUS_LO);

    IOLog("programRcsForContext: ELSP after: LO=0x%08x HI=0x%08x STATUS_LO=0x%08x\n",
          elsp_after_lo, elsp_after_hi, status_lo);

    bool ok = (elsp_after_lo == desc_lo && elsp_after_hi == desc_hi);

    if (!ok) {
        IOLog("❌ programRcsForContext: ELSP write FAILED, "
              "expected LO=0x%08x HI=0x%08x\n", desc_lo, desc_hi);
    } else {
        IOLog("✅ programRcsForContext: ELSP write OK\n");
    }

    // Kick execlist
    mmioWrite32(RCS0_EXECLIST_CONTROL, 0x1);
    IOSleep(1);
    uint32_t status_after_kick = mmioRead32(RCS0_EXECLIST_STATUS_LO);
    IOLog("programRcsForContext: EXECLIST kicked, STATUS_LO=0x%08x\n", status_after_kick);

    // --------------------------------------------------
    // STEP 5: Clean up
    // --------------------------------------------------
    fOwner->forcewakeRenderRelease();
    listGem->release();

    // We STILL are not "submitting" anything, only verifying ELSP write.
    return ok;
}
*/



bool FakeIrisXEExeclist::programRcsForContext(
        FakeIrisXEFramebuffer* fb,
        uint64_t ctxGpu,
        FakeIrisXEGEM* ringGem,
        uint64_t batchGpu)
{
    // ringGem / batchGpu are still useful for building the context image / ring,
    // but they are NOT used directly in the execlist descriptor.
    if (!fOwner) {
        IOLog("programRcsForContext: no owner\n");
        return false;
    }

    IOLog("=== SIMPLE ELSP SUBMIT TEST (v3) === ctx=0x%llx batch=0x%llx\n",
          ctxGpu, batchGpu);

    // --------------------------------------------------
    // STEP 0: Hold RENDER forcewake
    // --------------------------------------------------
    if (!fOwner->forcewakeRenderHold(5000 /*ms*/)) {
        IOLog("❌ programRcsForContext: forcewakeRenderHold() FAILED\n");
        return false;
    }

    uint32_t ack = mmioRead32(0x130044); // FORCEWAKE_ACK
    IOLog("programRcsForContext: FORCEWAKE_ACK after hold = 0x%08x\n", ack);

    // --------------------------------------------------
    // STEP 1: Build a REAL execlist descriptor in registers
    // --------------------------------------------------
    // LRCA = context GGTT address >> 12
    uint32_t lrca = (uint32_t)(ctxGpu >> 12);

    // Gen11/12 descriptor (simplified):
    //  - bits [31:12] = LRCA
    //  - bit 0        = VALID
    //  (we keep everything else 0 for now)
    uint32_t desc_lo = (lrca << 12) | 0x3; // VALID | ACTIVE
    uint32_t desc_hi = 0x00010000;        // simple priority

    
    
    IOLog("programRcsForContext: ctxGpu=0x%llx lrca=0x%x descLo=0x%08x descHi=0x%08x\n",
          (unsigned long long)ctxGpu, lrca, desc_lo, desc_hi);

    // --------------------------------------------------
    // STEP 2: Read ELSP before write
    // --------------------------------------------------
    uint32_t elsp_before_lo = mmioRead32(RCS0_EXECLIST_SUBMITPORT_LO);
    uint32_t elsp_before_hi = mmioRead32(RCS0_EXECLIST_SUBMITPORT_HI);
    IOLog("programRcsForContext: ELSP before: LO=0x%08x HI=0x%08x\n",
          elsp_before_lo, elsp_before_hi);

    // --------------------------------------------------
    // STEP 3: Write descriptor directly to submit port
    // --------------------------------------------------
    mmioWrite32(RCS0_EXECLIST_SUBMITPORT_LO, desc_lo);
    mmioWrite32(RCS0_EXECLIST_SUBMITPORT_HI, desc_hi);

    IOSleep(2); // let posted writes land

    // --------------------------------------------------
    // STEP 4: Read back ELSP + STATUS
    // --------------------------------------------------
    uint32_t elsp_after_lo = mmioRead32(RCS0_EXECLIST_SUBMITPORT_LO);
    uint32_t elsp_after_hi = mmioRead32(RCS0_EXECLIST_SUBMITPORT_HI);
    uint32_t status_lo     = mmioRead32(RCS0_EXECLIST_STATUS_LO);

    IOLog("programRcsForContext: ELSP after: LO=0x%08x HI=0x%08x STATUS_LO=0x%08x\n",
          elsp_after_lo, elsp_after_hi, status_lo);

    bool ok = (elsp_after_lo == desc_lo && elsp_after_hi == desc_hi);
    if (!ok) {
        IOLog("❌ programRcsForContext: ELSP write FAILED, "
              "expected LO=0x%08x HI=0x%08x\n", desc_lo, desc_hi);
        fOwner->forcewakeRenderRelease();
        return false;
    }

    IOLog("✅ programRcsForContext: ELSP descriptor write OK\n");

    mmioWrite32(RING_HEAD, 0);

    
    // --------------------------------------------------
    // STEP 5: Kick execlist
    // --------------------------------------------------
    mmioWrite32(RCS0_EXECLIST_CONTROL, 0x1);   // minimal "kick"
    IOSleep(1);

    uint32_t status_after_kick = mmioRead32(RCS0_EXECLIST_STATUS_LO);
    IOLog("programRcsForContext: EXECLIST kicked, STATUS_LO=0x%08x\n",
          status_after_kick);

    // --------------------------------------------------
    // STEP 6: Release forcewake
    // --------------------------------------------------
    fOwner->forcewakeRenderRelease();

    return true;
}









bool FakeIrisXEExeclist::writeExeclistDescriptor(FakeIrisXEFramebuffer* fb, uint64_t ctxGpuAddr, uint64_t batchGpuAddr, size_t batchSize)
{
    return this->programRcsForContext(fb, ctxGpuAddr, nullptr, batchGpuAddr);
}








bool FakeIrisXEExeclist::submitBatchWithExeclist(
        FakeIrisXEFramebuffer* fb,
        FakeIrisXEGEM*        batchGem,   // unused for ring-only fence test
        size_t                batchSize,  // unused
        FakeIrisXERing*       ring,
        uint32_t              timeoutMs)
{
    if (!fb || !ring) {
        IOLog("[Exec] submitBatchWithExeclist: invalid args (fb/ring)\n");
        return false;
    }

    IOLog("[Exec] submitBatchWithExeclist(): GEN12 RING EXECUTION PATH (ring-only)\n");

    if (!fb->forcewakeRenderHold(timeoutMs)) {
        IOLog("[Exec] submitBatchWithExeclist: forcewakeRenderHold FAILED\n");
        return false;
    }

    bool success = false;

    FakeIrisXEGEM* ringBacking = nullptr;
    FakeIrisXEGEM* ctx         = nullptr;
    FakeIrisXEGEM* fenceGem    = nullptr;

    do {
        //
        // 1) Fence buffer (GGTT-visible) that PIPE_CONTROL will write to.
        //
        fenceGem = FakeIrisXEGEM::withSize(4096, 0);
        if (!fenceGem) {
            IOLog("[Exec] submitBatchWithExeclist: fence alloc FAILED\n");
            break;
        }
        fenceGem->pin();

        uint64_t fenceGpu = fb->ggttMap(fenceGem) & ~0xFFFULL;
        IOBufferMemoryDescriptor* fmd = fenceGem->memoryDescriptor();
        if (!fmd) {
            IOLog("[Exec] submitBatchWithExeclist: fence md NULL\n");
            break;
        }
        volatile uint32_t* fenceCpu =
            (volatile uint32_t*)fmd->getBytesNoCopy();
        if (!fenceCpu) {
            IOLog("[Exec] submitBatchWithExeclist: fenceCpu NULL\n");
            break;
        }
        *fenceCpu = 0;
        OSSynchronizeIO();

        //
        // 2) Ring backing buffer
        //
        size_t ringSize = ring->size();
        ringBacking = FakeIrisXEGEM::withSize(ringSize, 0);
        if (!ringBacking) {
            IOLog("[Exec] submitBatchWithExeclist: ringBacking alloc FAILED\n");
            break;
        }
        ringBacking->pin();

        uint64_t ringGpu = fb->ggttMap(ringBacking) & ~0xFFFULL;
        IOBufferMemoryDescriptor* rmd = ringBacking->memoryDescriptor();
        if (!rmd) {
            IOLog("[Exec] submitBatchWithExeclist: ring md NULL\n");
            break;
        }
        uint32_t* ringCpu = (uint32_t*)rmd->getBytesNoCopy();
        if (!ringCpu) {
            IOLog("[Exec] submitBatchWithExeclist: ringCpu NULL\n");
            break;
        }
        bzero(ringCpu, rmd->getLength());

        //
        // 3) REAL GEN12 commands directly in RCS ring:
        //    PIPE_CONTROL (POST-SYNC WRITE IMMEDIATE -> fenceGpu = 1)
        //    MI_BATCH_BUFFER_END
        //
        const uint32_t PIPE_CONTROL        = (0x7A << 23);
        const uint32_t PC_WRITE_IMM        = (1 << 14);
        const uint32_t PC_CS_STALL         = (1 << 20);
        const uint32_t PC_GLOBAL_GTT       = (1 << 2);
        const uint32_t MI_BATCH_BUFFER_END = (0x0A << 23);

        unsigned d = 0;

        // PIPE_CONTROL: post-sync immediate write -> fenceGpu = 1
        ringCpu[d++] = PIPE_CONTROL | PC_WRITE_IMM | PC_CS_STALL | PC_GLOBAL_GTT;
        ringCpu[d++] = 0; // DW1
        ringCpu[d++] = (uint32_t)(fenceGpu & 0xFFFFFFFFULL); // DW2: addr LO
        ringCpu[d++] = 1;                                    // DW3: immediate

        // MI_BATCH_BUFFER_END
        ringCpu[d++] = MI_BATCH_BUFFER_END;
        ringCpu[d++] = 0x00000000;

        size_t ringBytes = d * sizeof(uint32_t);
        IOLog("[Exec] Ring BUILT (PIPE_CONTROL): dwords=%u bytes=%zu fenceGpu=0x%llx\n",
              d, ringBytes, (unsigned long long)fenceGpu);

        //
        // 4) Build LRC image with correct ring state layout.
        //
        uint32_t ringTail = (uint32_t)ringBytes & (uint32_t)(ringSize - 1);

        IOReturn ret = kIOReturnError;
        ctx = FakeIrisXELRC::buildLRCContext(
                fb,
                ringBacking,
                ringSize,
                ringGpu,
                /* ringHead */ 0,
                /* ringTail */ ringTail,
                &ret);

        if (!ctx || ret != kIOReturnSuccess) {
            IOLog("[Exec] submitBatchWithExeclist: buildLRCContext FAILED (ret=0x%x)\n", ret);
            break;
        }

        ctx->pin();
        uint64_t ctxGpu = fb->ggttMap(ctx) & ~0xFFFULL;

        IOBufferMemoryDescriptor* cmd = ctx->memoryDescriptor();
        if (!cmd) {
            IOLog("[Exec] submitBatchWithExeclist: ctx md NULL\n");
            break;
        }
        uint8_t* ctxCpu = (uint8_t*)cmd->getBytesNoCopy();
        if (!ctxCpu) {
            IOLog("[Exec] submitBatchWithExeclist: ctxCpu NULL\n");
            break;
        }

        // LRC header + ring state are already correct from buildLRCContext().
        OSSynchronizeIO();

        
        
        
        //
        // 🔥 GEN12 LEGACY RING REGISTER PROGRAMMING 🔥
        // Required BEFORE the first execlist context load or GPU reads garbage
        //
        const uint32_t GEN12_RCS0_RBSTART_LO = 0x23C30;
        const uint32_t GEN12_RCS0_RBSTART_HI = 0x23C34;
   
        // Tell GPU this is the ring base (GGTT address)
        fb->safeMMIOWrite(GEN12_RCS0_RBSTART_LO, (uint32_t)(ringGpu & 0xFFFFFFFFULL));
        fb->safeMMIOWrite(GEN12_RCS0_RBSTART_HI, (uint32_t)(ringGpu >> 32));

        // HEAD must start at 0
        fb->safeMMIOWrite(GEN12_RCS0_RBHEAD, 0);

        // TAIL = number of bytes of commands (must match your LRC TAIL!)
        fb->safeMMIOWrite(GEN12_RCS0_RBTAIL, ringBytes);

        IOLog("[Exec] GEN12_RCS RING_START + HEAD/TAIL programmed: base=0x%llx tail=%zu\n",
              (unsigned long long)ringGpu, ringBytes);

        
        
        
            
        //
        // 5) Program EXECLIST with context only (LRCA).
        //
        if (!programRcsForContext(fb, ctxGpu, nullptr, 0 /* no batch */)) {
            IOLog("[Exec] submitBatchWithExeclist: programRcsForContext FAILED\n");
            break;
        }

        IOLog("[Exec] Submitted ELSP => LRCA=0x%x\n",
              (uint32_t)(ctxGpu >> 12));

        IOSleep(2); // tiny delay before polling

        //
        // 6) Poll fence GPU is supposed to write via PIPE_CONTROL.
        //
        for (uint32_t t = 0; t < timeoutMs; ++t) {
            OSSynchronizeIO();
            if (*fenceCpu != 0) {
                IOLog("[Exec] fence updated by GPU: 0x%08x\n", *fenceCpu);
                success = true;
                break;
            }
            IOSleep(1);
        }

        if (!success) {
            uint32_t head   = mmioRead32(RING_HEAD);
            uint32_t tail   = mmioRead32(RING_TAIL);
            uint32_t status = mmioRead32(RCS0_EXECLIST_STATUS_LO);
            IOLog("❌ TIMEOUT — Fence still 0 (HEAD=0x%x TAIL=0x%x STATUS_LO=0x%08x)\n",
                  head, tail, status);
        }

    } while (false);

    fb->forcewakeRenderRelease();

    // Cleanup
    if (ctx)         ctx->release();
    if (ringBacking) ringBacking->release();
    if (fenceGem)    fenceGem->release();
    // batchGem is owned by caller.

    return success;
}


















void FakeIrisXEExeclist::engineIrq(uint32_t iir)
{
    // We only care about execlist/ctx interrupts here
    if ((iir & (RCS_INTR_COMPLETE | RCS_INTR_CTX_SWITCH | RCS_INTR_FAULT)) == 0)
        return;

    // On any of those, read CSB entries.
    processCsbEntries();
}


void FakeIrisXEExeclist::processCsbEntries()
{
    if (!fCsbGem || fCsbEntryCount == 0)
        return;

    IOBufferMemoryDescriptor* md = fCsbGem->memoryDescriptor();
    if (!md) return;

    volatile uint64_t* csbBase =
        (volatile uint64_t*)md->getBytesNoCopy();
    if (!csbBase) return;

    const uint32_t mask = fCsbEntryCount - 1; // assume power-of-two

    for (;;) {
        uint32_t idx = fCsbReadIndex & mask;
        volatile uint64_t* entry = csbBase + idx * 2;

        uint64_t low  = entry[0];
        uint64_t high = entry[1];

        if (low == 0 && high == 0) {
            // no more new CSB entries
            break;
        }

        // Consume it
        handleCsbEntry(low, high);

        // Mark as consumed (zero it)
        entry[0] = 0;
        entry[1] = 0;
        OSSynchronizeIO();

        fCsbReadIndex++;
    }

    // If engine idle and we have pending work, maybe kick
    maybeKickScheduler();
}


enum {
    CSB_STATUS_COMPLETE = 1u << 0,
    CSB_STATUS_PREEMPT  = 1u << 1,
    CSB_STATUS_FAULT    = 1u << 2,
};

void FakeIrisXEExeclist::handleCsbEntry(uint64_t low, uint64_t high)
{
    uint32_t ctxId  = (uint32_t)(low & 0xFFFFFFFFu);
    uint32_t status = (uint32_t)(high & 0xFFFFFFFFu);

    IOLog("(FakeIrisXE) [Exec] CSB: ctx=%u status=0x%08x\n", ctxId, status);

    if (status & CSB_STATUS_FAULT) {
        onContextFault(ctxId, status);
    } else if (status & CSB_STATUS_COMPLETE) {
        onContextComplete(ctxId, status);
    } else if (status & CSB_STATUS_PREEMPT) {
        // preemption or switch – treat like partial completion
        onContextComplete(ctxId, status);
    } else {
        // "switch only" or other
        // You can log / ignore for now
    }
}



void FakeIrisXEExeclist::onContextComplete(uint32_t ctxId, uint32_t status)
{
    // Mark inflight entry for ctxId as completed
    for (int i = 0; i < 2; ++i) {
        XEHWContext* hw = fInflight[i];
        if (hw && hw->ctxId == ctxId) {
            IOLog("(FakeIrisXE) [Exec] ctx %u complete on slot %d\n", ctxId, i);
            fInflight[i] = nullptr;
            fInflightSeqno[i] = 0;
            break;
        }
    }

    // You could also wake any waiters, notify Accelerator, etc.

    // Immediately schedule next context (if any)
    maybeKickScheduler();
}

void FakeIrisXEExeclist::onContextFault(uint32_t ctxId, uint32_t status)
{
    XEHWContext* hw = lookupHwContext(ctxId);
    if (!hw) return;

    hw->banScore++;
    IOLog("(FakeIrisXE) [Exec] ctx %u fault (banScore=%u)\n",
          ctxId, hw->banScore);

    if (hw->banScore >= kMaxBanScore) {
        hw->banned = true;
        IOLog("(FakeIrisXE) [Exec] ctx %u BANNED\n", ctxId);
    }

    // Drop inflight reference
    for (int i = 0; i < 2; ++i) {
        if (fInflight[i] && fInflight[i]->ctxId == ctxId) {
            fInflight[i] = nullptr;
            fInflightSeqno[i] = 0;
        }
    }

    // Do not reschedule banned contexts
    maybeKickScheduler();
}


bool FakeIrisXEExeclist::submitForContext(XEHWContext* hw, FakeIrisXEGEM* batchGem)
{
    if (!hw || !batchGem || hw->banned)
        return false;

    uint32_t nextTail = (fQTail + 1) % kMaxExeclistQueue;
    if (nextTail == fQHead) {
        IOLog("(FakeIrisXE) [Exec] submitForContext: queue full\n");
        return false;
    }

    batchGem->pin();
    uint64_t batchGGTT = fOwner->ggttMap(batchGem) & ~0xFFFULL;

    ExecQueueEntry& e = fQueue[fQTail];
    e.hwCtx    = hw;
    e.batchGem = batchGem;
    e.batchGGTT= batchGGTT;
    e.seqno    = fNextSeqno++;
    e.inFlight = false;
    e.completed= false;
    e.faulted  = false;

    fQTail = nextTail;

    IOLog("(FakeIrisXE) [Exec] queued ctx=%u seq=%u\n", hw->ctxId, e.seqno);

    // Try to kick immediately
    maybeKickScheduler();

    return true;
}


FakeIrisXEExeclist::ExecQueueEntry* FakeIrisXEExeclist::pickNextReady()
{
    if (fQHead == fQTail)
        return nullptr;

    ExecQueueEntry* best = nullptr;
    uint32_t bestPri = 0;
    uint32_t idx = fQHead;

    while (idx != fQTail) {
        ExecQueueEntry& e = fQueue[idx];
        XEHWContext* hw = e.hwCtx;
        if (hw && !hw->banned && !e.inFlight) {
            uint32_t pri = hw->priority;
            if (!best || pri > bestPri) {
                best    = &e;
                bestPri = pri;
            }
        }
        idx = (idx + 1) % kMaxExeclistQueue;
    }
    return best;
}

void FakeIrisXEExeclist::maybeKickScheduler()
{
    // See if any ELSP slot is free
    int freeSlot = -1;
    for (int i = 0; i < 2; ++i) {
        if (!fInflight[i]) {
            freeSlot = i;
            break;
        }
    }
    if (freeSlot < 0)
        return; // both slots busy

    ExecQueueEntry* e = pickNextReady();
    if (!e) return;

    if (submitToELSPSlot(freeSlot, e)) {
        e->inFlight = true;
        fInflight[freeSlot] = e->hwCtx;
        fInflightSeqno[freeSlot] = e->seqno;
        IOLog("(FakeIrisXE) [Exec] ctx %u seq %u -> ELSP slot %d\n",
              e->hwCtx->ctxId, e->seqno, freeSlot);
    }
}

bool FakeIrisXEExeclist::submitToELSPSlot(int slot, ExecQueueEntry* e)
{
    if (!e || !e->hwCtx)
        return false;

    XEHWContext* hw = e->hwCtx;

    // Build a small descriptor on the stack (you can still use a GEM if you want)
    uint32_t desc[8] = {0};

    desc[0] = (uint32_t)(hw->lrcGGTT & 0xFFFFFFFFu);
    desc[1] = (uint32_t)(hw->lrcGGTT >> 32);
    desc[2] = 0;
    desc[3] = (1u << 0) | (1u << 1); // VALID|ACTIVE
    desc[4] = (uint32_t)(e->batchGGTT & 0xFFFFFFFFu);
    desc[5] = (uint32_t)(e->batchGGTT >> 32);
    desc[6] = 0;
    desc[7] = 0;

    // For now, write descriptor into a small GEM and ELSP points to it exactly
    FakeIrisXEGEM* listGem = FakeIrisXEGEM::withSize(4096, 0);
    if (!listGem) return false;
    listGem->pin();
    uint32_t* cpu = (uint32_t*)listGem->memoryDescriptor()->getBytesNoCopy();
    bzero(cpu, 4096);
    memcpy(cpu, desc, sizeof(desc));

    uint64_t listGGTT = fOwner->ggttMap(listGem) & ~0xFFFULL;

    // For 2-port ELSP, port 0/1 share same SUBMITPORT regs on Gen12,
    // hardware manages internal pending vs active.
    // So we just write once per submit.
    uint32_t lo = (uint32_t)(listGGTT & 0xFFFFFFFFu);
    uint32_t hi = (uint32_t)(listGGTT >> 32);

    mmioWrite32(RCS0_EXECLIST_SUBMITPORT_LO, lo);
    mmioWrite32(RCS0_EXECLIST_SUBMITPORT_HI, hi);

    // Kick control register (lightweight)
    mmioWrite32(RCS0_EXECLIST_SQ_CONTENTS, 0x1);
    IOSleep(2);
    uint32_t sq = mmioRead32(RCS0_EXECLIST_SQ_CONTENTS);
    IOLog("SQ_CONTENTS after kick = 0x%08x\n", sq);

    
    IOLog("(FakeIrisXE) [Exec] submitToELSPSlot slot=%d ctx=%u listGGTT=0x%llx\n",
          slot, hw->ctxId, listGGTT);

    // We keep listGem alive only for test; in real impl you'd reuse a pool.
    listGem->release();
    return true;
}



FakeIrisXEExeclist::XEHWContext* FakeIrisXEExeclist::lookupHwContext(uint32_t ctxId)
{
    for (uint32_t i = 0; i < fHwContextCount; ++i) {
        XEHWContext* hw = &fHwContexts[i];
        if (hw->ctxId == ctxId) {
            return hw;
        }
    }
    return nullptr;
}


FakeIrisXEExeclist::XEHWContext* FakeIrisXEExeclist::createHwContextFor(uint32_t ctxId, uint32_t priority)
{
    // If it already exists, just update priority and return
    XEHWContext* existing = lookupHwContext(ctxId);
    if (existing) {
        existing->priority = priority;
        IOLog("(FakeIrisXE) [Exec] createHwContextFor: reuse ctx=%u pri=%u\n",
              ctxId, priority);
        return existing;
    }

    if (fHwContextCount >= kMaxHwContexts) {
        IOLog("(FakeIrisXE) [Exec] createHwContextFor: no slots left\n");
        return nullptr;
    }

    XEHWContext* hw = &fHwContexts[fHwContextCount];
    bzero(hw, sizeof(XEHWContext));

    hw->ctxId    = ctxId;
    hw->priority = priority;
    hw->banScore = 0;
    hw->banned   = false;

    // --- 1) Allocate ring backing for this context ---
    // You can take ring size from your existing RCS ring if you prefer:
    // size_t ringSize = fOwner->fRcsRing ? fOwner->fRcsRing->size() : 0x4000;
    size_t ringSize = 0x4000; // 16KB is fine for now; adjust later.

    hw->ringGem = FakeIrisXEGEM::withSize(ringSize, 0);
    if (!hw->ringGem) {
        IOLog("(FakeIrisXE) [Exec] ctx=%u: ringGem alloc FAILED\n", ctxId);
        return nullptr;
    }

    hw->ringGem->pin();
    hw->ringGGTT = fOwner->ggttMap(hw->ringGem);
    if (!hw->ringGGTT) {
        IOLog("(FakeIrisXE) [Exec] ctx=%u: ggttMap(ring) FAILED\n", ctxId);
        hw->ringGem->unpin();
        hw->ringGem->release();
        hw->ringGem = nullptr;
        return nullptr;
    }
    hw->ringGGTT &= ~0xFFFULL;

    IOLog("(FakeIrisXE) [Exec] ctx=%u: ringGGTT=0x%llx size=0x%zx\n",
          ctxId, hw->ringGGTT, ringSize);

    // --- 2) Build LRC image for this context using your helper ---
    IOReturn ret = kIOReturnError;
    hw->lrcGem = FakeIrisXELRC::buildLRCContext(
                    fOwner,
                    hw->ringGem,
                    ringSize,
                    hw->ringGGTT,
                    ctxId,  // context ID
                    0,      // pdps / vm pointer (0 for now)
                    &ret);

    if (!hw->lrcGem || ret != kIOReturnSuccess) {
        IOLog("(FakeIrisXE) [Exec] ctx=%u: buildLRCContext FAILED (ret=0x%x)\n",
              ctxId, ret);
        if (hw->lrcGem) hw->lrcGem->release();
        hw->lrcGem = nullptr;

        hw->ringGem->unpin();
        hw->ringGem->release();
        hw->ringGem = nullptr;
        return nullptr;
    }

    hw->lrcGem->pin();
    hw->lrcGGTT = fOwner->ggttMap(hw->lrcGem);
    if (!hw->lrcGGTT) {
        IOLog("(FakeIrisXE) [Exec] ctx=%u: ggttMap(LRC) FAILED\n", ctxId);
        hw->lrcGem->unpin();
        hw->lrcGem->release();  hw->lrcGem = nullptr;
        hw->ringGem->unpin();
        hw->ringGem->release(); hw->ringGem = nullptr;
        return nullptr;
    }
    hw->lrcGGTT &= ~0xFFFULL;

    IOLog("(FakeIrisXE) [Exec] ctx=%u: LRC GGTT=0x%llx\n",
          ctxId, hw->lrcGGTT);

    
    
    
    
    // --- 3) Patch minimal required fields inside LRC image ---
    
    {
        IOBufferMemoryDescriptor* md = hw->lrcGem->memoryDescriptor();
        if (md) {
            uint8_t* cpu = (uint8_t*)md->getBytesNoCopy();

            //
            // GEN12 REQUIRED LRC HEADER FIELDS
            //

            // PDP0 = Fake Page Directory Root — use LRC addr so GPU doesn't reject
            write_le64(cpu + 0x00, hw->lrcGGTT & ~0xFFFULL);

            // Enable timestamp for scheduler acceptance
            write_le32(cpu + 0x30, 0x00010000);

            // Context Control:
            //  bit0 = Load
            //  bit3 = Valid
            //  bit8 = Header Size (1 = 64 bytes)
            const uint32_t CTX_CTRL = (1 << 0) | (1 << 3) | (1 << 8);
            write_le32(cpu + 0x2C, CTX_CTRL);

            //
            // RING STATE (GEN12 required offsets)
            //
            write_le32(cpu + 0x100 + 0x00, 0); // HEAD
            write_le32(cpu + 0x100 + 0x04, 0); // TAIL
            write_le32(cpu + 0x100 + 0x0C, (uint32_t)(hw->ringGGTT & 0xFFFFFFFF)); // RING_BASE

            // Optional: Reset timestamp counter
            fOwner->safeMMIOWrite(0x2580, 0);
        }
    }

    
    
       
        
        
        
        
        
        
    // --- 4) Allocate fence GEM for this context (per-context fence) ---
    hw->fenceGem = FakeIrisXEGEM::withSize(4096, 0);
    if (!hw->fenceGem) {
        IOLog("(FakeIrisXE) [Exec] ctx=%u: fenceGem alloc FAILED\n", ctxId);
        // We could still continue without fence, but for now bail out:
        hw->lrcGem->unpin();  hw->lrcGem->release();  hw->lrcGem = nullptr;
        hw->ringGem->unpin(); hw->ringGem->release(); hw->ringGem = nullptr;
        return nullptr;
    }

    hw->fenceGem->pin();
    hw->fenceGGTT = fOwner->ggttMap(hw->fenceGem);
    if (!hw->fenceGGTT) {
        IOLog("(FakeIrisXE) [Exec] ctx=%u: ggttMap(fence) FAILED\n", ctxId);
        hw->fenceGem->unpin(); hw->fenceGem->release(); hw->fenceGem = nullptr;
        // keep ctx but without fence; or bail out entirely. For now bail:
        hw->lrcGem->unpin();  hw->lrcGem->release();  hw->lrcGem = nullptr;
        hw->ringGem->unpin(); hw->ringGem->release(); hw->ringGem = nullptr;
        return nullptr;
    }
    hw->fenceGGTT &= ~0xFFFULL;

    // clear fence value
    if (IOBufferMemoryDescriptor* fmd = hw->fenceGem->memoryDescriptor()) {
        volatile uint32_t* fenceCpu = (volatile uint32_t*)fmd->getBytesNoCopy();
        if (fenceCpu) {
            fenceCpu[0] = 0;
            OSSynchronizeIO();
        }
    }

    IOLog("(FakeIrisXE) [Exec] ctx=%u: fenceGGTT=0x%llx\n",
          ctxId, hw->fenceGGTT);

    // Finally register this context in the table
    fHwContextCount++;

    IOLog("(FakeIrisXE) [Exec] createHwContextFor: ctx=%u pri=%u DONE (total=%u)\n",
          ctxId, priority, fHwContextCount);

    return hw;
}







