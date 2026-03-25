# Changelog: aerolamp-v1.2 (from main)

## V1.2 Board Support
- **Board detection** (`board.c`, `board.h`): Detects V1.1 vs V1.2 at startup by looking for at least 3V on either the 12V rail (V1.1) or the 24V rail (V1.2).
- **Power sequencing**: V1.2 powers up 24V boost first (from VSYS), then 12V buck (from 24V). V1.1 retains original order (12V first, then 24V boost). Shutdown order is reversed for each board.
- **Board-aware UI**: Debug screen shows board version. PSU error screen shows board-appropriate requirements (V1.1: "12V | 1.8A", V1.2: "20W at 5-24V").

## USB-PD Step-Down Negotiation
- **Step-down loop** (`usbpd.c`): Tries voltage candidates from highest to lowest until one meets minimum current. V1.2: 20V→15V→12V→9V→5V. V1.1: 12V only (safety — 20V on V1.1 12V rail is dangerous).
- **Voltage AND current verification**: Checks actual VBUS voltage (via ADC) and negotiated current before accepting a candidate. Prevents false acceptance when a source falls back to 5V but reports adequate current.
- **Capability mismatch detection**: If the PD source sets the capability_mismatch flag in the RDO, returns 0mA — the source can't guarantee the requested current.
- **Barrel jack detection**: Skips PD negotiation when no USB-C is connected. Configures safe PDOs in the STUSB4500 so any USB hot-plug negotiates a board-safe voltage.
- **Hot-plug detection** (`usbpd_update()`): Detects USB insertion at runtime and writes safe PDOs to override STUSB4500 NVM defaults.
- **V1.2 PD adequacy check**: V1.2 regulators produce valid output from 5V at no load, so voltage checks alone can't catch inadequate sources. V1.2 path in `lamp_power_up_rails()` rejects if USB-PD negotiation failed.

## Power Monitoring
- **Dual-rail checking**: `lamp_is_power_ok()` now verifies both 12V (10.5–13.5V) and 24V (21.0–27.0V) rails are in range, AND that both rail enable flags are set. Prevents false positives from barrel jack voltage leaking to sense pins with rails off.
- **Dual-rail fault check**: Runtime fault detection in `lamp_update()` monitors both rails. Previously only checked 12V.
- **PSU error screen with live voltages**: Shows VBUS, 12V, and 24V readings on the error screen. "Retrying..." status appears at bottom during retry attempts.

## Power Retry Loop
- **Automatic retry**: When power is inadequate, retries `lamp_power_up_rails()` every 3 seconds. If power recovers (e.g., barrel jack plugged in), the lamp starts automatically.
- **Display feedback**: PSU error screen updates with live voltage readings. "Retrying..." message shown during retry attempts.

## Lamp Type Detection
- **Dimming-response test**: Requests 70% power, waits for strike and warmup, then checks if ballast responds to dimming. Replaces the previous two-phase test.
- **Unknown type UI**: If type test can't determine ballast type, main screen shows "LAMP TYPE COULD NOT BE DETERMINED" with DEBUG button still accessible.
- **Retest button**: Debug screen has RETEST button that clears stored type, re-runs detection, and reboots. Buttons hidden during retest.
- **Type test serial diagnostics**: Prints frequency and reported power level during dimming check for debugging.

## Watchdog
- **Hardware watchdog**: 1500ms timeout, enabled after USB-PD negotiation but before lamp power-up. Catches runtime hangs (brownout gray zone) and forces clean reboot. Feeds in main loop, type test loops, and `lamp_power_up_rails()` sleeps.

## Debug Display
- **Board version**: Shows "V1.1" or "V1.2" inline with lamp type.
- **12V label**: Shows "Switched" (V1.1) or "Reg" (V1.2) based on board type.
- **USB info**: Shows requested voltage, actual VBUS voltage, and negotiated current when USB is connected. Shows "USB: none (barrel jack)" otherwise.

## Known Issues
- **Blocking operations in main loop**: `lamp_power_up_rails()`, `lamp_perform_type_test()`, `usbpd_negotiate()`, and the 12V soft-start ramp block the main loop during retry. Future work: convert to non-blocking state machines for shorter watchdog timeout.
- **V1.1 barrel jack re-insertion UI desync**: When powered over 5V USB-C, inserting and removing a barrel jack twice leaves the lamp off but the UI showing power ON. Toggling the power switch in the UI corrects it.
- **V1.1 barrel-to-5V handover, basic vs dimmable**: When removing barrel jack with 5V USB connected, dimmable (I3) lamp transitions smoothly to the incompatible power supply screen. Basic (I2) lamp causes a reboot first.
- **Barrel-to-USB handover with non-20V sources (V1.2)**: Sources without a 20V PDO (e.g., Pi 27W at 15V max) negotiate only 5.2V/1.5A on handover, then reboot and complain. USB cold start negotiates correctly (e.g., 15.1V/1.4A from the same supply).
- **Lamp retest on barrel power**: When retesting a dimmable lamp on barrel jack power, boots to insufficient power error screen. Unplugging and replugging fixes it.
- **Board type misdetects v1.1 on reboot**: When rebooting from a lamp test, v1.1 boards incorrectly detect as v1.2 boards