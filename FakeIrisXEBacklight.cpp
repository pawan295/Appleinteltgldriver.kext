//
//  FakeIrisXEBacklight.cpp
//  FakeIrisXEFramebuffer
//
//  Created by Anomy on 01/12/25.
//

#include "FakeIrisXEBacklight.hpp"

#include "FakeIrisXEBacklight.hpp"
#include <IOKit/IOLib.h>


#define super IOService

#include "FakeIrisXEFramebuffer.hpp"

OSDefineMetaClassAndStructors(FakeIrisXEBacklight, IOService);

bool FakeIrisXEBacklight::init(OSDictionary* dict) {
    if (!super::init(dict)) return false;
    fOwnerFB = nullptr;
    fBrightness = 100;
    fMaxBrightness = 100;
    return true;
}

bool FakeIrisXEBacklight::start(IOService* provider) {
    if (!super::start(provider)) return false;

    
    
    // store provider (should be your framebuffer instance)
    // find display0 under the framebuffer
    OSIterator* it = provider->getChildIterator(gIOServicePlane);
    if (it) {
        IOService* child = nullptr;
        while ((child = OSDynamicCast(IOService, it->getNextObject()))) {
            if (!strcmp(child->getName(), "display0")) {
                fOwnerFB = child->getProvider();   // get framebuffer
                break;
            }
        }
        it->release();
    }

    if (!fOwnerFB) fOwnerFB = provider; // fallback

    
    
    
    
    
    // publish typical properties macOS expects
    setProperty("AppleBacklightDisplay", kOSBooleanTrue);

    // publish numeric properties. create an OSNumber, set and release.
    OSNumber* nMax = OSNumber::withNumber((uint64_t)fMaxBrightness, 32);
    if (nMax) {
        setProperty("max-brightness", nMax);
        nMax->release();
    }

    OSNumber* nCur = OSNumber::withNumber((uint64_t)fBrightness, 32);
    if (nCur) {
        setProperty("brightness", nCur);
        nCur->release();
    }

    // Optional: give nominal nits so some apps show physical value
    OSNumber* nNits = OSNumber::withNumber((uint64_t)100, 32);
    if (nNits) { setProperty("IOBacklightNits", nNits); nNits->release(); }

    
    // in FakeIrisXEBacklight::start(...)
    setProperty("IOProviderClass", "IODisplayConnect");       // provider type
    setProperty("IONameMatch", "AppleBacklightDisplay");     // so AppleBacklightDisplay will match this node
    setProperty("AAPL,backlight-control", kOSBooleanTrue);   // advertise backlight control capability

    // brightness properties that UI reads:
    OSNumber* maxN = OSNumber::withNumber((uint64_t)100, 32);
    OSNumber* curN = OSNumber::withNumber((uint64_t)fBrightness, 32);
    if (maxN) { setProperty("max-brightness", maxN); maxN->release(); }
    if (curN) { setProperty("brightness", curN);    curN->release(); }

    // also publish vendor/display-index hints if available:
    setProperty("AAPL,backlight-index", OSNumber::withNumber((uint64_t)1, 32) ); // optional

    // Register so system discovers it
    registerService();

    IOLog("[FakeIrisXEBacklight] started (brightness=%u)\n", fBrightness);
    return true;
}

void FakeIrisXEBacklight::stop(IOService* provider) {
    IOLog("[FakeIrisXEBacklight] stop\n");
    super::stop(provider);
}

void FakeIrisXEBacklight::free() {
    super::free();
}

// Called by system (or our user-client). Update registry, call into framebuffer.
IOReturn FakeIrisXEBacklight::setBrightnessInternal(uint32_t level) {
    if (level > fMaxBrightness) level = fMaxBrightness;
    fBrightness = level;

    // update registry property (so UI sees current value)
    OSNumber* nCur = OSNumber::withNumber((uint64_t)fBrightness, 32);
    if (nCur) {
        setProperty("brightness", nCur);
        nCur->release();
    }


    // Now call into the framebuffer to actually set PWM / registers.
    if (fOwnerFB) {
        FakeIrisXEFramebuffer* fb = OSDynamicCast(FakeIrisXEFramebuffer, fOwnerFB);
        if (fb) {
            // you must implement this method in FakeIrisXEFramebuffer:
            fb->setBacklightPercent(level);
            IOLog("[FakeIrisXEBacklight] forwarded brightness=%u to framebuffer\n", fBrightness);
            return kIOReturnSuccess;
        } else {
            IOLog("[FakeIrisXEBacklight] provider is not FakeIrisXEFramebuffer\n");
            return kIOReturnUnsupported;
        }
    }

    return kIOReturnNotAttached;
}


// Add near other methods in FakeIrisXEBacklight.cpp
IOReturn FakeIrisXEBacklight::setProperties(OSObject *properties) {
    IOLog("[FakeIrisXEBacklight] setProperties() called\n");

    if (!properties) return super::setProperties(properties);

    OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
    if (!dict) {
        IOLog("[FakeIrisXEBacklight] setProperties(): not a dict\n");
        return super::setProperties(properties);
    }

    // 1) Direct "brightness" = OSNumber
    OSObject *obj = dict->getObject("brightness");
    if (obj) {
        OSNumber *num = OSDynamicCast(OSNumber, obj);
        if (num) {
            uint32_t v = num->unsigned32BitValue();
            IOLog("[FakeIrisXEBacklight] setProperties(): brightness (direct) = %u\n", v);
            return setBrightnessInternal(v);
        }
    }

    // 1b) Direct "vblm" = OSNumber (AppleDisplay parameter key)
    obj = dict->getObject("vblm");
    if (obj) {
        OSNumber *num = OSDynamicCast(OSNumber, obj);
        if (num) {
            uint32_t v = num->unsigned32BitValue();
            IOLog("[FakeIrisXEBacklight] setProperties(): vblm (direct) = %u\n", v);
            return setBrightnessInternal(v);
        }
    }

    // 2) Some clients send "IODisplayParameters" => { "brightness": { "value": <num> } }
    obj = dict->getObject("IODisplayParameters");
    if (obj) {
        OSDictionary *params = OSDynamicCast(OSDictionary, obj);
        if (params) {
            OSObject *b = params->getObject("brightness");
            OSDictionary *bDict = OSDynamicCast(OSDictionary, b);
            if (bDict) {
                OSObject *valObj = bDict->getObject("value");
                OSNumber *valNum = OSDynamicCast(OSNumber, valObj);
                if (valNum) {
                    uint32_t v = valNum->unsigned32BitValue();
                    IOLog("[FakeIrisXEBacklight] setProperties(): brightness (IODisplayParameters.value) = %u\n", v);
                    return setBrightnessInternal(v);
                }
            }
            // fallback: sometimes "brightness" is OSNumber directly under IODisplayParameters
            OSNumber *num = OSDynamicCast(OSNumber, params->getObject("brightness"));
            if (num) {
                uint32_t v = num->unsigned32BitValue();
                IOLog("[FakeIrisXEBacklight] setProperties(): brightness (IODisplayParameters, direct) = %u\n", v);
                return setBrightnessInternal(v);
            }

            // AppleDisplay may use "vblm"
            OSObject *vObj = params->getObject("vblm");
            OSDictionary *vDict = OSDynamicCast(OSDictionary, vObj);
            if (vDict) {
                OSObject *valObj = vDict->getObject("value");
                OSNumber *valNum = OSDynamicCast(OSNumber, valObj);
                if (valNum) {
                    uint32_t v = valNum->unsigned32BitValue();
                    IOLog("[FakeIrisXEBacklight] setProperties(): vblm (IODisplayParameters.value) = %u\n", v);
                    return setBrightnessInternal(v);
                }
            }
            OSNumber *vNum = OSDynamicCast(OSNumber, params->getObject("vblm"));
            if (vNum) {
                uint32_t v = vNum->unsigned32BitValue();
                IOLog("[FakeIrisXEBacklight] setProperties(): vblm (IODisplayParameters, direct) = %u\n", v);
                return setBrightnessInternal(v);
            }
        }
    }

    // Not handled — pass to superclass
    IOLog("[FakeIrisXEBacklight] setProperties(): no brightness key found\n");
    return super::setProperties(properties);
}
