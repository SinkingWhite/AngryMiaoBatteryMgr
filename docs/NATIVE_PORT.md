# Native Win32 port — 0.2.0

Tested device: AngryMiao INFINITY MOUSE .100 only.

The primary executable is now native C++20 / Win32 using the Windows HID, SetupAPI, Shell notification-area, GDI/GDI+ and standard controls APIs. MSVC `/MT` statically links the C++ runtime; there is no .NET, WPF, Qt or downloaded runtime dependency. Windows-supplied DLLs remain dependencies.

Only the native implementation is included in the current repository. Native settings use an INI file; JSON preferences from earlier releases are not imported. The same singleton and Run registry value prevent two versions from running together. Existing opt-in startup is preserved and repointed to the native executable on launch; absent startup is not enabled automatically.

## Validation, 2026-10-30

- Release MSVC x64 build succeeded with `/W4 /WX`, no warnings.
- 24 native checks passed: checksum, battery offsets/header/ranges, empty response, zero, DPI table, stage bounds, immutable input and byte preservation, stale/concurrent readback rejection and canceled enumeration.
- Native hardware read: mouse 98%, spare 86%, active stage 2 / X=800 / Y=800.
- Native DPI switch: 800 -> 1200 -> 800 passed with readback and unchanged-byte checks; original stage restored.
- Native GUI smoke opened popup, read data and exited normally (exit 0).
- The actual Win32 popup was captured and inspected: compact light layout, readable rows and standard controls, no card layout or dark mode.

The last percentages are receiver observations, not a new physical accuracy certification. The spare-battery jump remains unresolved. Native sleep/unplug stress, Windows login and mixed-DPI screens remain manual tests. Synchronous HID driver stalls can still delay cancellation. CI has been configured for native compilation and protocol tests; remote execution is not claimed here.

Final EXE: 339,968 bytes (332 KiB). The whitelist source archive was extracted into a fresh directory and built successfully; all 24 native checks passed there. Final GUI smoke also exited 0. Import inspection found Windows system DLLs only; no .NET or VC++ runtime DLL dependency. The native build is running locally.
