//
//  FakeIrisXELRC.hpp
//  FakeIrisXEFramebuffer
//
//  Created by Anomy on 01/12/25.
//
// FakeIrisXELRC.hpp
#pragma once
#include <IOKit/IOLib.h>
#include "FakeIrisXEGEM.hpp"
#include "FakeIrisXEFramebuffer.hpp"


class FakeIrisXELRC {
public:
    // Build an LRC context image (returns a pinned FakeIrisXEGEM containing the context image)
    // pageSize is 4096.
    // The context image layout is simple & legal for Gen11: header + ring state + ring backing pointers.
    static FakeIrisXEGEM* buildLRCContext(
           FakeIrisXEFramebuffer* fb,
           FakeIrisXEGEM*         ringGem,
           size_t                 ringSize,
           uint64_t               ringGpuAddr,
           uint32_t               ringHead,
           uint32_t               ringTail,
           IOReturn*              outErr);
   
    
    static void write_le32(uint8_t* p, uint32_t v) { *(uint32_t*)p = v; }
    static void write_le64(uint8_t* p, uint64_t v) { *(uint64_t*)p = v; }



};
