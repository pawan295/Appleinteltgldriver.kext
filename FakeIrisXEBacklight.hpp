//
//  FakeIrisXEBacklight.hpp
//  FakeIrisXEFramebuffer
//
//  Created by Anomy on 01/12/25.
//
#ifndef FAKE_IRIS_XE_BACKLIGHT_HPP
#define FAKE_IRIS_XE_BACKLIGHT_HPP

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>

class FakeIrisXEBacklight : public IOService {
    OSDeclareDefaultStructors(FakeIrisXEBacklight);

private:
    IOService* fOwnerFB;      // parent framebuffer (we'll OSDynamicCast to your FB class)
    uint32_t  fBrightness;    // 0..100
    uint32_t  fMaxBrightness; // max, default 100

public:
    virtual bool init(OSDictionary* = nullptr) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual void free() override;

    // Called by system / user clients to change brightness (0..100)
    IOReturn setBrightnessInternal(uint32_t level);

    // Optional helpers
    uint32_t getBrightnessInternal() const { return fBrightness; }
    uint32_t getMaxBrightness() const { return fMaxBrightness; }
    
    IOReturn setProperties(OSObject* properties)override;
    
    
};

#endif // FAKE_IRIS_XE_BACKLIGHT_HPP
