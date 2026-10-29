# Verification

The current program is native C++ / Win32 and has only been tested on **AngryMiao INFINITY MOUSE .100**.

- MSVC x64 Release build succeeded with `/MT /O2 /W4 /WX` and no warnings.
- 21 native protocol checks passed, including response identity, battery range validation, DPI table parsing, checksums, stage bounds, preservation of unrelated bytes, immutable input and cancellation.
- Real-device battery and X/Y DPI reads succeeded.
- DPI switching 800 -> 1200 -> 800 succeeded with read-back verification and original-stage restoration.
- GUI smoke opened the popup, read data and exited with code 0.
- The light native popup was visually inspected.
- A source archive extracted into a fresh directory built successfully without external repository files.
- Import inspection found Windows system DLLs only, with no .NET or VC++ runtime DLL dependency.

Spare-battery jumps remain unresolved. Sleep, Windows login, prolonged operation, mixed-DPI monitors and broader device/firmware compatibility are not certified. CI does not access real hardware.
