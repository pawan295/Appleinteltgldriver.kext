#ifndef FAKE_IRIS_XE_ACCEL_SHARED_H
#define FAKE_IRIS_XE_ACCEL_SHARED_H

#include <IOKit/IOService.h>

#include <IOKit/IOTypes.h>     // basic IOKit typedefs

#include <IOKit/IOService.h>


enum {
    kFakeIris_Method_GetCaps                = 0,
    kFakeIris_Method_CreateContext          = 1,
    kFakeIris_Method_DestroyContext         = 2,
    kFakeIris_Method_BindSurfaceUserMapped  = 3,
    kFakeIris_Method_PresentContext         = 4,

    // New for Phase 8:
    kFakeIris_Method_SubmitExeclistFenceTest = 5,
};

#define kFakeIris_Method_SubmitExeclistFenceTest  7



//
// ===== ACCEL Selec
enum {
    kAccelSel_Ping = 0,
    kAccelSel_GetCaps = 1,
    kAccelSel_CreateContext = 2,
    kAccelSel_Submit = 3,
    kAccelSel_Flush = 4,
    kAccelSel_DestroyContext = 5,
    kAccelSel_BindSurface = 6,
    kAccelSel_InjectTest = 10,      // debug
};


struct XECtx {
    uint32_t ctxId;
    bool alive;
    uintptr_t surf_vaddr;   // kernel-side pointer (mapped via clientMemory / IOConnectMapMemory64)
    size_t surf_bytes;
    uint32_t surf_rowbytes; // bytes per row in source
    uint32_t surf_w;
    uint32_t surf_h;
    // optional: more flags
};



//
// ===== Capability struct =====
//
struct XEAccelCaps {
    uint32_t version;          // = 1
    uint32_t metalSupported;   // 0 or 1
    uint32_t reserved0;
    uint32_t reserved1;
};

//
// ===== Context Create =====
//
struct XECreateCtxIn {
    uint32_t flags;
    uint32_t pad;
    uint64_t sharedGPUPtr;
};

struct XECreateCtxOut {
    uint32_t ctxId;
    uint32_t pad;
};

//
// ===== Submit =====
//
struct XESubmitIn {
    uint32_t ctxId;
    uint32_t numBytes;
};

//
// ===== Surface Bind =====
//
struct XEBindSurfaceIn {
  uint32_t ctxId;
  uint32_t ioSurfaceID;
  uint32_t width;
  uint32_t height;
  uint32_t bytesPerRow;
  uint32_t surfaceID;   // <<< THIS EXISTS!
  void*    cpuPtr;
  uint64_t gpuAddr;
  uint32_t pixelFormat;
  bool valid;
};


struct XEPresentPayload {
    uint32_t ioSurfaceID;   // IOSurface ID created in userspace
    uint32_t x;             // dest x in fb
    uint32_t y;             // dest y in fb
    uint32_t w;             // width
    uint32_t h;             // height
};



struct XEBindSurfaceOut {
    uint64_t gpuAddr;      // not used
    uint32_t status;
    uint32_t reserved;
};

//
// ===== Command Opcodes =====
//
enum : uint32_t {
    XE_CMD_NOP    = 0,
    XE_CMD_CLEAR  = 1,    // payload: uint32_t colorARGB
    XE_CMD_RECT   = 2,    // payload: XERectPayload
    XE_CMD_COPY   = 3,    // payload: XECopyPayload
    XE_CMD_FLUSH  = 4,    // no payload
    XE_CMD_PRESENT = 5    // (future use)
};

//
// ===== Command Payloads =====
//
struct XERectPayload {
    uint32_t x, y;
    uint32_t w, h;
    uint32_t colorARGB;
};

struct XECopyPayload {
    uint32_t sx, sy;
    uint32_t dx, dy;
    uint32_t w, h;
};

//
// ===== Ring Header (simple linear ring) =====
//
struct __attribute__((packed)) XEHdr {
    uint32_t magic;       // XE_MAGIC
    uint32_t version;     // XE_VERSION
    uint32_t capacity;    // usable payload size (bytes)
    uint32_t head;        // producer offset (user)
    uint32_t tail;        // consumer offset (kernel)
    uint32_t reserved[3];
};

//
// ===== Command Header =====
//
struct __attribute__((packed)) XECmd {
    uint32_t opcode;     // XE_CMD_*
    uint32_t bytes;      // payload size
    uint32_t ctxId;      // context
    uint32_t reserved;   // padding/alignment
};

struct XEClearPayload { uint32_t color; };

struct XEContext {
    void*     surfCPU;      // mapped CPU pointer from IOSurfaceInKernelMemory
    uint32_t  surfWidth;
    uint32_t  surfHeight;
    uint32_t  surfRowBytes;
};



//
// ===== Constants =====
//
static constexpr uint32_t XE_MAGIC   = 0x53524558u;  // 'XERS'
static constexpr uint32_t XE_VERSION = 1;
static constexpr uint32_t XE_PAGE    = 4096;

static inline uint32_t xe_align(uint32_t v) {
    return (v + 3u) & ~3u;   // 4-byte align
}
        
        
        
        
        

    


#endif
