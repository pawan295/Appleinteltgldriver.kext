# AppleIntelTGL

Intel Tiger Lake (Gen12) Graphics Driver for macOS

## Overview

This is a graphics driver for Intel Tiger Lake (Gen12) integrated GPUs, designed for macOS. 

##Current Limitations
Currently it only opens accelrator user clients for surface and display releted, no command sumbission and metal.
Need support from experts......


## Features

- **GPU Command Submission**: Hardware-accelerated rendering via GuC (Graphics Microcontroller)
- **Memory Management**: GEM (Graphics Execution Manager) and GTT (Graphics Translation Table)
- **Display Support**: Framebuffer, mode setting, and DP (DisplayPort).
- **Metal Integration**: Full Metal framework support for GPU acceleration (Needs work)
- **Power Management**: Runtime PM and GT power management
- **Interrupt Handling**: GT and display interrupt processing

## System Requirements

- macOS (10.15+)
- Intel Tiger Lake GPU (Device IDs: 0x9a40-0x9a78)
- Supported PCI Device: `8086:9A49`

## Project Structure

```
AppleIntelTGLController/
├── AppleIntelTGLController.cpp/h    # Main driver controller
├── AppleIntelTGLEGLFramebuffer.cpp/h # Framebuffer service
├── AppleIntelTGLIOAccelerator.cpp/h # IOAccelerator for Metal
├── AppleIntelTGLIOAcceleratorClients.cpp/h # Client implementations
├── AppleIntelTGLIOSurfaceManager.cpp/h # IOSurface integration
├── Intel*.cpp/h                     # Core GPU subsystems
└── Info.plist                       # KEXT configuration
```

## Building

1. Open `AppleIntelTGLController.xcodeproj` in Xcode
2. Select the appropriate build configuration
3. Build the project

## Installation

1. Build the kext
2. Copy to `/Library/Extensions/`
3. Rebuild kernel cache: `sudo kextcache -i /`
4. Reboot

## Driver Components

| Component | Description |
|----------|-------------|
| `AppleIntelTGLController` | Main driver service, PCI device handling |
| `AppleIntelTGLEGLFramebuffer` | Framebuffer for display output |
| `AppleIntelTGLIOAccelerator` | Metal GPU acceleration |
| `IntelGuC` | GuC firmware management |
| `IntelGEM` | Graphics memory management |
| `IntelRingBuffer` | Command ring buffer |

## Configuration

The driver is configured via `Info.plist`:

```xml
<key>IOPCIPrimaryMatch</key>
<string>0x9A498086</string>
```

## Supported Features

- GPU memory allocation (GEM objects)
- DisplayPort link training
- GuC-based command submission
- Power state management

## License

This project is provided for educational and development purposes.
