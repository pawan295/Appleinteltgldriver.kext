//
//  FakeIrisXELRC.cpp
//  FakeIrisXEFramebuffer
//
//  Created by Anomy on 01/12/25.
//

// FakeIrisXELRC.cpp
#include "FakeIrisXELRC.hpp"
#include "FakeIrisXEGEM.hpp"
#include "FakeIrisXERing.h"
#include "i915_reg.h"



FakeIrisXEGEM* FakeIrisXELRC::buildLRCContext(
    FakeIrisXEFramebuffer* fb,
    FakeIrisXEGEM* ringGem,
    size_t ringSize,
    uint64_t ringGpuAddr,
    uint32_t ringHead,
    uint32_t ringTail,
    IOReturn* outErr)
{
    if (outErr) *outErr = kIOReturnError;
    if (!fb || !ringGem) return nullptr;

    const size_t ctxSize = 4096;
    FakeIrisXEGEM* ctxGem = FakeIrisXEGEM::withSize(ctxSize, 0);
    if (!ctxGem) return nullptr;

    ctxGem->pin();
    IOBufferMemoryDescriptor* md = ctxGem->memoryDescriptor();
    if (!md) {
        ctxGem->unpin();
        ctxGem->release();
        return nullptr;
    }

    uint8_t* p = (uint8_t*)md->getBytesNoCopy();
    if (!p) {
        ctxGem->unpin();
        ctxGem->release();
        return nullptr;
    }
    bzero(p, ctxSize);

    uint64_t ctxGpu = fb->ggttMap(ctxGem) & ~0xFFFULL;

    //
    // ===== GEN12 LRC HEADER =====
    //

    // Fake PDP0 = context page, so HW sees a non-zero root and doesn't reject.
    write_le64(p + 0x00, ctxGpu & ~0xFFFULL);  // PDP0
    write_le64(p + 0x08, 0);                   // PDP1
    write_le64(p + 0x10, 0);                   // PDP2
    write_le64(p + 0x18, 0);                   // PDP3

    // Timestamp enable
    write_le32(p + 0x30, 0x00010000);

    // CONTEXT_CONTROL:
    //  bit0 = Load
    //  bit3 = Valid
    //  bit8 = Header size (1 => 64 bytes)
    const uint32_t CTX_CTRL = (1u << 0) | (1u << 3) | (1u << 8);
    write_le32(p + 0x2C, CTX_CTRL);

    //
    // ===== GEN12 RING STATE BLOCK =====
    //
    const uint32_t ringStateOff = 0x100;

    // HEAD / TAIL are byte offsets from RING_BASE, masked by (ringSize - 1).
    uint32_t headBytes = ringHead & (uint32_t)(ringSize - 1);
    uint32_t tailBytes = ringTail & (uint32_t)(ringSize - 1);

    write_le32(p + ringStateOff + 0x00, headBytes);  // RING_HEAD
    write_le32(p + ringStateOff + 0x04, tailBytes);  // RING_TAIL

    // RING_BASE (GGTT VA of ring buffer)
    write_le32(p + ringStateOff + 0x08, (uint32_t)(ringGpuAddr & 0xFFFFFFFFu));      // LO
    write_le32(p + ringStateOff + 0x0C, (uint32_t)(ringGpuAddr >> 32));             // HI

    // RING_CTL:
    //   bits [20:12] = (num_pages - 1)
    //   bit 0        = Ring Enable
    uint32_t pages = (uint32_t)(ringSize / 4096);
    if (!pages) pages = 1;
    uint32_t ringCtl = ((pages - 1) << 12) | 1u;
    write_le32(p + ringStateOff + 0x10, ringCtl);

    __sync_synchronize();
    OSSynchronizeIO();

    IOLog("GEN12 LRC built: ctxGpu=0x%llx ringGpu=0x%llx head=%u tail=%u pages=%u ctl=0x%08x\n",
          (unsigned long long)ctxGpu,
          (unsigned long long)ringGpuAddr,
          headBytes, tailBytes, pages, ringCtl);

    if (outErr) *outErr = kIOReturnSuccess;
    return ctxGem;
}
