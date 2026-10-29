# Changelog

## 0.2.0 — Unreleased

- Native C++/Win32 single-EXE rewrite with static runtime, approximately 332 KB.
- Preserves cat-paw tray, light popup, preferences, polling and verified DPI switching.
- The current repository contains only the native implementation.
- Uses native.ini/native.log; old JSON is preserved but not imported.

## 0.1.0 — Unreleased

- Windows tray battery monitoring with a color-coded cat-paw icon.
- Compact light popup with dual battery readings, preferences and opt-in startup.
- Current X/Y DPI display and confirmed switching between enabled device stages.
- Native HID transport, polling recovery, local rotating diagnostics and executable regression checks.

Known issue: spare battery telemetry can drop unexpectedly and recover after reseating; root cause remains open. See README.md for additional limitations.
