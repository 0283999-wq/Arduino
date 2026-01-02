# SPECTRA_V2_CLEAN (firmware)

- Hardware: ESP32 NodeMCU-32S, circular 240x240 SPI TFT, DFPlayer Mini, 3x TTP223 touch buttons, 2x mechanical volume buttons (pins in `firmware/config.h`).
- Display: load your RGB565 skin header as `firmware/vinyl_ui.h`; `firmware/vinyl_assets.h` maps the bitmap to the UI renderer.
- DFPlayer SD layout: folders `01..99`, each holding files `001.mp3`..`255.mp3` (e.g., `/01/001.mp3`). Use `DEFAULT_TRACK` in `config.h` to choose startup track.
- Controls: touch LEFT=Prev, touch MIDDLE=Play/Pause, touch RIGHT=Next; mechanical LEFT=Vol–, RIGHT=Vol+. Hold both mechanical buttons 2s toggles Bluetooth UI mode.
- Volume overlay stays visible after adjustments; hold volume buttons to ramp faster (repeat acceleration).
- Bluetooth UI: distinct screen with clock/battery/animated bar; no DFPlayer commands in BT mode.
- Clock: attempts RTC/NTP integration stub; if no valid time, UI shows `--:--` until set via Serial (`HH:MM`).
- UI redraw policy: background skin drawn once per mode; overlays refresh on change to avoid flicker.
- Power: battery sense on `PIN_BATTERY_SENSE` with 100k/100k divider; thresholds defined in `config.h`.
- Serial boot log: prints `=== SPECTRA SETUP START/END ===` to confirm initialization.
