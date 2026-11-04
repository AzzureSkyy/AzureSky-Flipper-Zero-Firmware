# AzureSky Firmware

Custom Flipper Zero firmware based on the latest official firmware, with enhancements focused on animation variety and larger animation playlists.

## What changed

### 1) Non-repeating idle animations
Idle animations now use a shuffle-bag style selector:
- Valid animations are played without repeats until all have been shown once.
- After a full cycle, a new cycle starts and avoids immediate back-to-back repeats.
- Selection still honors manifest `Weight` values within each cycle.

### 2) Combined manifest support for longer playlists
You can combine multiple animation packs into one larger playlist by merging their manifest entries and keeping all referenced animation folders together on SD card.
- Place your combined manifest at: `/ext/dolphin/manifest.txt` (or your chosen animation folder path).
- Ensure every `Name:` entry has a matching animation directory with required assets.
- Keep `Min/Max level` and `Min/Max butthurt` ranges aligned so entries stay eligible together.

### 3) Infrared Morse Beacon app
Added a new built-in app: **IR Morse Beacon**.
- Sends Morse code as an IR beacon loop (start/stop from app UI).
- Includes quick message presets and live switching.
- Supports TX routing for external IR modules: **Auto**, **Internal**, or **Ext PA7**.

## Building

From repository root:
- `./fbt` (Linux/macOS)
- `./fbt.cmd` (Windows)

To build an update package for qFlipper:
- `./fbt updater_package`

## Flashing

- qFlipper: **Install from file** and choose the generated `.tgz` package.
- On-device update: copy update folder to SD card and run `update.fuf`.

## License

This repository is licensed under **GNU GPL v3.0**. See `LICENSE`.
