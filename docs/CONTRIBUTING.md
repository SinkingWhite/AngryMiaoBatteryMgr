# Contributing

The primary implementation is `native/` (C++20 / Win32). Install Visual Studio 2022 Build Tools with Desktop development with C++ and Windows SDK, then run `./scripts/build.ps1` in PowerShell. The build uses `/W4 /WX` and runs the native protocol checks without real hardware. Optional formatting uses clang-format and .clang-format.

Keep parsing/report construction in protocol.hpp, native transport in hid.hpp, and UI/lifecycle in app.cpp. Protocol changes require evidence for interface selection, lengths, report ID, commands and response identity. Stage writes must read a fresh table, preserve unrelated bytes and verify readback. Never add device writes to CI or automatic startup tests.

`native-tests.exe --hardware` reads the real receiver; `--switch-test` deliberately changes a stage and restores it. Close the tray app and official web driver before manual hardware tests. Record unsupported or unverified cases rather than reporting them as passed.


Local upstream checkouts, downloaded vendor scripts, captures and build outputs are excluded from Git and source archives. Inspect shared logs for personal information. Preserve license notices. Contributions use GPL-3.0-only.
