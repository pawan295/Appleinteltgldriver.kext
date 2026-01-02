# AppleintelTGLDriver.kext


Working on custom kext to get Intel Iris XE graphics works on hackintosh. Inviting Devloper to work on this project and let start Iris XE on HAckintosh

Source files are not updated regularly.......

you can help in this project.




what is working in latest version:

PHASE 1 – Load a custom kext

Status: COMPLETED

Kext loads without signature issues

IOService start() works

Info.plist matching is stable

PHASE 2 – Map MMIO + wake GT

Status:  COMPLETED

BAR0 mapped

FORCEWAKE works

FORCEWAKE_ACK confirmed

GT power wells enabled

GT clock domains awaken

GPU is alive.

PHASE 3 – Build a minimal framebuffer

Status: COMPLETED

IOFramebuffer subclass loads

Framebuffer allocated

WindowServer sees our display device

macOS boots to GUI using our framebuffer

have real working display powered entirely by our custom kext.

PHASE 4 – Enable the entire Tiger Lake display pipeline

Status:  FULLY COMPLETED

Pipe A

Transcoder A

Plane 1A

ARGB8888

1920×1080

60 Hz

Stride 7680

eDP panel lit by our code

Internal display runs using our driver.
Screen corruption / 2-split issue fixed.

PHASE 5 – Accelerator framework

Status:  COMPLETED (major part)

FakeIrisXEAccelerator published

IOAccelerator properties exposed

Metal shows “Supported”

AcceleratorUserClient attaches

Shared ring buffer implemented

User-space tools can ping the accelerator

FB mapping → user space works

Kernel sees CLEAR commands

✔ Accelerator stack is working end-to-end.



The remaining steps are:

1. GEM buffer objects (Done)
2. GGTT binder  (Done)
3. Command streamer ring (Done)
4. Execlists context (Done) currently not working
5. GuC firmware (Stub)
6. BLT engine (Future)
7. 3D pipeline
8. Metal integration
These are huge, but not unknown.
We follow Linux i915 (which is open-source).
We follow Intel PRM Vol15–17

