# AzureSky Firmware

Custom Flipper Zero firmware based on the latest official firmware, with enhancements focused on animation variety and larger animation playlists.

## What changed

### 1) Non-repeating idle animations
Idle animations now use a shuffle-bag style selector:
- Valid animations are played without repeats until all have been shown once.
- After a full cycle, a new cycle starts and avoids immediate back-to-back repeats.
- Selection still honors manifest `Weight` values within each cycle.

### 2) Themes: switchable animation sets
A **Themes** app in the main menu lets you switch between whole animation sets, one at a time.
- Location: **Main menu -> Themes**.
- **Dolphin** (the official animations from `/ext/dolphin`) is always listed first.
- Every folder in `/ext/themes` that has its own `manifest.txt` is listed after it. The folder name is the theme name.
- Pick a theme and press **Apply**. The Flipper reboots and plays only that theme's animations.
- The current theme is marked with `*`. If a theme folder is missing or broken, it falls back to Dolphin.

Each theme is self-contained, with its own manifest and animation folders:

```
SD card
└── themes
    ├── AzureSky
    │   ├── manifest.txt
    │   ├── Animation_One_128x64
    │   │   ├── meta.txt
    │   │   └── frame_0.bm ...
    │   └── ...
    └── Miscellaneous
        ├── manifest.txt
        └── ...
```

To add a theme, drop its folder into `/ext/themes`:
- `manifest.txt` must sit directly inside the theme folder.
- Every `Name:` entry in the manifest needs a matching animation folder with `meta.txt` and its `frame_N.bm` files.
- To make a longer playlist, put several packs' animation folders in one theme folder and merge their manifest entries into its `manifest.txt`.
- Don't create a `themes/Dolphin` folder. Dolphin is built in.
- `themes/.current` is created by the app to remember your choice.

Firmware updates only replace `/ext/dolphin`, so theme folders are never overwritten.

### 3) Morse Beacon integrated into Infrared app
Morse Beacon is now built into the existing **Infrared** app menu (not a separate standalone app).
- Location: **Infrared -> Morse Beacon**.
- Supports custom text entry from the device UI.
- Includes adjustable WPM speed control.
- Supports TX routing for external IR modules: **Auto**, **Internal**, or **Ext PA7**.
- **Save Text to File** saves the current message as its own file in `/ext/subghz/morse_beacon/`, named after the message (for example `SOS_HELP.txt`). Each different message gets its own file; saving the same message again updates that file.
- **Load Text from File** opens a scrollable list of your saved messages in `/ext/subghz/morse_beacon/`. Pick one and it becomes the current message. You can also drop your own pre-typed `.txt` files into that folder.

### 4) Version
The firmware reports itself as **6.0-AzureSky** in qFlipper, the mobile apps and the About screen. The RPC protocol version is read from `assets/protobuf/Changelog`, so qFlipper and the mobile apps connect normally regardless of repo tags.

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
