# Native hardware checks

Only **AngryMiao INFINITY MOUSE .100** has been tested. Other devices or firmware require fresh validation.

## Identify and read

Use `Get-PnpDevice -PresentOnly | Where-Object InstanceId -match 'VID_3151&PID_5007'`. The command target must be MI_02 / UsagePage FFFF / Usage 0002 / feature length 65. Other collections are not safe targets for probing.

Build from the repository root with `./scripts/build.ps1`. Close the tray application and official web driver before running `./artifacts/native/native-tests.exe --hardware`; it issues the audited battery and DPI reads. Compare readings with independent references. Three vendor bars do not establish an exact percentage.

## Tray and popup

Run Miao.exe normally. Confirm the cat-paw icon, compact battery/DPI tooltip and light popup. Check colors at 30/31/50/51 boundaries using injected reports or a controlled test build; do not alter the device merely to force percentages. Verify closing focus hides the popup while polling continues. Toggle tooltip and hidden-start preferences, reopen and restart to confirm persistence in native.ini. Change polling interval and verify subsequent reads.

## DPI writes

Record original stage, stage values and colors. Run `native-tests.exe --switch-test` only when deliberately testing hardware writes: it switches to the next enabled stage, verifies read-back, then restores the original. Verify the original is restored even if testing fails. Also test the popup dropdown. Check other stage values/colors remain unchanged and invalid/unconfirmed responses are reported honestly. Never add this command to automated CI.

## Recovery and shutdown

Unplug the receiver, confirm unknown status instead of fake 0%, reconnect and wait for the polling/backoff window or click refresh. Repeat during a read and during sleep/resume. Test mouse sleep separately from receiver disconnection. Test Windows Explorer restart, mixed-DPI monitors and screen edges. The icon should return after Explorer restarts.

Toggle opt-in startup intentionally and inspect HKCU/Software/Microsoft/Windows/CurrentVersion/Run/AngryMiaoBattery. Verify login, turn it off and confirm removal. Exit through the tray menu and check the process, icon and handles disappear. A synchronous Windows HID call can delay shutdown if the driver stalls.

## Diagnostics and open issue

Native error logs are in LOCALAPPDATA/AngryMiaoBattery/native.log. Review logs before sharing them. Record firmware, connection mode, reproduction steps and whether another driver was open. Do not install drivers, disable security or assume elevation is needed as a default troubleshooting step.

Spare percentages may drop with the same cell in place and recover after reseating; this remains unresolved. Capture state before reseating. Previously verified: receiver capability identification, battery/DPI reads, native stage switch/restoration and GUI startup/shutdown. Broader sleep, unplug stress, startup-login and long-duration acceptance remain pending.
