# AngryMiaoBatteryMgr

[中文（默认）](../README.md) | **English**

An unofficial native Windows tray utility for the **AngryMiao INFINITY MOUSE .100**.

> **Real-device testing has only been performed on the AngryMiao INFINITY MOUSE .100.** Other models and firmware versions are unverified. Similar protocols do not guarantee compatibility. The application currently has a Chinese UI; English UI support is planned.

## Features

- Cat-paw tray icon: **red at 0–30%, orange at 31–50%, green at 51–100%** mouse charge; gray question mark for unknown state.
- Compact tooltip: `⚡️97%/89%·800` means mouse charge / spare charge · current DPI. Unavailable values appear as `?`.
- A compact light-gray popup near the tray; click outside to dismiss it.
- Current X/Y DPI and switching between existing enabled stages with read-back confirmation, preserving other stage values and colors.
- Auto-saved polling interval, tooltip, hidden-start and opt-in Windows startup preferences.
- Serialized background reads, retry backoff and reconnection without keeping the official web driver open.

## Run

Targets **Windows 10/11 x64**. Download a published build from [Releases](https://github.com/SinkingWhite/AngryMiaoBatteryMgr/releases), if available; otherwise build from source.

Place `Miao.exe` in a permanent folder and run it. Find the paw in the notification area or its overflow menu. Left-click opens the popup; right-click offers refresh and exit. Startup is off by default. Previously enabled startup is preserved and repointed to this EXE; run the app again after moving it to update that path.

## Build and verify

Install Visual Studio 2022 Build Tools with **Desktop development with C++** and Windows SDK, then run from the repository root:

```powershell
./scripts/build.ps1
```

The build uses `/MT /O2 /W4 /WX`, statically links the C++ runtime, runs 21 hardware-independent protocol checks and produces `artifacts/native/Miao.exe` plus a corresponding-source archive. No machine-specific absolute path is required.

Close the tray app and official web driver before explicit hardware checks:

```powershell
./artifacts/native/native-tests.exe --hardware     # Read batteries and DPI
./artifacts/native/native-tests.exe --switch-test  # Change stage, then restore it
./artifacts/native/Miao.exe --smoke-test            # Open, read and exit; no DPI write
```

Verified: native compilation, 21 protocol checks, real-device battery/DPI reads, 800 → 1200 → 800 stage switching/restoration, and popup startup/shutdown. Sleep, prolonged operation, mixed-DPI monitors and other firmware need further testing. GitHub Actions builds and runs hardware-independent checks only.

## Settings and limitations

Preferences are stored in `%LOCALAPPDATA%/AngryMiaoBattery/native.ini`. Errors go to `native.log` in that folder, rotated at 1 MiB. Polling defaults to 30 seconds, configurable from 5 to 3600 seconds; failure backoff is capped at 300 seconds. No accounts, cloud service or telemetry. Review logs before sharing them.

- **Spare charge has been reported to drop unexpectedly with the same cell in place and recover after reseating. The cause remains unresolved.** Receiver-reported spare percentages are not guaranteed to stay accurate.
- Avoid simultaneous configuration changes in the official web driver; clients may compete for the receiver channel.
- Failed reads show unknown rather than false 0%. A stalled synchronous HID driver call can delay shutdown.
- The icon belongs to the notification area, not a pinned taskbar icon with custom text.

## Repository

`native/` contains the application, protocol, HID transport and checks. `scripts/` contains build/package scripts. `docs/` contains protocol, testing and contributor documentation. `LICENSES/` retains third-party notices; `.github/` contains Windows CI.

[Contributing](CONTRIBUTING.md) · [Changelog](CHANGELOG.md) · [Protocol](PORTING_ANALYSIS.md) · [Native verification](NATIVE_PORT.md)

## License and credits

[GPL-3.0-only](../LICENSE); retain [NOTICE](../NOTICE.md) and third-party license notices. Protocol work references [miaobat](https://github.com/LukePeltier/miaobat), [HaloBattery](https://github.com/HeyOkay/HaloBattery) and [AJAZZ Control Center](https://github.com/Aiacos/ajazz-control-center). DPI transactions were checked against the [official AM30 web driver](https://ammaster.angrymiao.com/mouse/); vendor scripts are not distributed here.

This project is not affiliated with Angry Miao and is not an official driver. Distribute corresponding source and build scripts with binaries.

## TODO

- [ ] **EN Support**: English application UI.
- [ ] More features: gradually support more functionality available in the official Angry Miao driver.
- [ ] Validate more devices and firmware versions.

These are roadmap directions, not shipped features or delivery-date commitments.
