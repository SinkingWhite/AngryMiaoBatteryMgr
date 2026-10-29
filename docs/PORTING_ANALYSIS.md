# Protocol audit

## Scope and provenance

Real-device testing is limited to **AngryMiao INFINITY MOUSE .100**. Repository code is native C++ / Win32. Other models and firmware are unverified.

- [miaobat](https://github.com/LukePeltier/miaobat), revision `1a7d4ee6c19a1a4e48722cba2f97243ebbdc0b70`, GPL-3.0: original battery protocol. Its `src/read_battery.c` defines VID/PID 3151:5007, Linux input2, 65-byte buffer, initialization 00 F7, and Linux battery offsets 2/10. README, source, Makefile, udev rule and license were reviewed. It does not validate returned ID/length/ranges or provide persistent retry.
- [HaloBattery](https://github.com/HeyOkay/HaloBattery/blob/2251f0534a8e886c9692ead42056059b4f69d8c0/providers/am_infinity.py), MIT: evidence for Windows report-ID handling, status 05 and mouse offset 3.
- [AJAZZ Control Center](https://github.com/Aiacos/ajazz-control-center/blob/03a29f7a90dd32057ca7cd0d6e6fa8f13d5400f3/src/devices/mouse/src/aj_series.cpp), GPL: additional protocol reference. Its early BIT7/direct-query assumptions did not retrieve this device's DPI table.
- [Official AM30 web driver](https://ammaster.angrymiao.com/mouse/am30), inspected asset `/mouse/assets/index-359d92a5.js`: authoritative evidence for AM30 receiver forwarding and command checksum. Proprietary JavaScript is not distributed here.

## Interface and battery

SetupAPI enumerates the confirmed MI_02 interface. Before writing, HidD_GetAttributes must match VID 3151 / PID 5007, and HidP_GetCaps must match UsagePage FFFF, Usage 0002 and FeatureReportByteLength 65. Mouse and keyboard collections are not command targets. The length comes from Windows capabilities, not a Linux buffer-size assumption.

HidD_SetFeature sends a 65-byte Windows buffer with report ID 00, command F7 at offset 1 and zero padding. After 50 ms, HidD_GetFeature requests ID 05. Offsets include the Windows report-ID byte: mouse=3, spare=11. Header padding, ID, nonempty response and 0–100 ranges are checked. Failure is unknown, not fabricated zero. HidD_GetFeature returns Boolean rather than actual payload length; the complete per-ID descriptor has not been extracted.

Captured sample:

```
050000480100010200000164000100F4000100F300140000000A05000000000000000000000000000000000000000000000100F9010100F9FF0100F5010100F5FF
```

Offsets 3/11 give 72/100, independently confirmed by the owner at that point. Direct Linux offsets gave incorrect 0/1. This single-point comparison does not certify long-term accuracy. Same-cell spare jumps and recovery after reseating remain unresolved. Missing-battery in-range sentinels and telemetry freshness remain unverified.

## DPI read and stage switch

The official page's `Mt` builds a 64-byte body: opcode, up to six arguments, then body[7] = `255 - (sum(body[0..6]) & 255)`. Native Windows buffers prepend report ID 00.

`dongleForward` implements: F6 05 to request routing; poll F7 until ready; FE 40 to set payload length; send command; poll F7 for response-ready; FC to retrieve it. Cleanup sends F6 00. All transactions run serially on the worker thread.

Read current profile with 85, then D4 for that profile. A DPI response must echo D4 and the expected profile, with 1–8 stages and an active index within bounds. Active index is Windows offset 3, count is 4; X values are little-endian words at 9+2*i, Y values at 25+2*i.

```
00D40001040000002B90012003B0044006800C00191027307590012003B0044006800C001910273075FF0000FF3200FFFF0000FF0000FFFF0000FFFF00FFFFFFFF
```

This response decodes to enabled stages 400/800/1200/1600, active index 1 (second stage), X=Y=800.

Switching reads a fresh table and copies the entire response, changing only D4->54, active index and header checksum. Other DPI values, colors, stage count and unknown bytes are preserved. After writing, read-back must match the target index/profile and all unrelated bytes. Up to four additional reads at 200 ms intervals tolerate briefly stale firmware responses. Real-device 800->1200->800 switching/restoration passed. Concurrent configuration edits can still race; do not run competing clients.

## Resources and remaining validation

Each sample re-enumerates and closes its native handle. SetupAPI lists and preparsed data are released. A single background thread serializes battery and DPI traffic; messages transfer results to the UI thread. Cancellation interrupts waits and exit joins the worker. Native synchronous HID calls can still stall inside the driver and delay shutdown.

Polling defaults to 30 seconds with bounded failure backoff. The popup and compact tooltip share current readings; failed reads do not present old data as confirmed. Windows notification interface v4 requires NIF_SHOWTIP for the standard tooltip.

Sleep, long-duration operation, native unplug stress, mixed-DPI monitors, absent-spare semantics and other firmware require additional testing. See HARDWARE_TESTING.md and NATIVE_PORT.md. Preserve upstream license notices when distributing the GPL-3.0-only application and its corresponding source.

F7 receiver state at Windows offset 1 may legally be 00 (idle) or 01 (forwarded response ready). Both retain battery fields at 3/11. A real 05 01 00 5C ... 60 frame was initially rejected after DPI polling. The parser now accepts only these two documented states, keeps zero padding at offset 2, rejects unknown states and checks for an empty payload excluding the state byte. Regression checks cover response-ready battery readings, bad padding and response-ready empty telemetry.
