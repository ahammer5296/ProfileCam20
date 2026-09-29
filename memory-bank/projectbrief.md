# Project Brief

Rebuild the legacy ProfileCam20 camera preview application as a small Windows-only portable utility.

## Supported target

- Windows 10 x64, including LTSC 2019 and LTSC 2021.
- Ordinary USB/UVC webcams; Logitech C525 and C920 are the first test devices.
- One running instance and one active camera at a time.
- Distribution is `ProfileCam20.exe` plus `settings.ini`. No installer and no administrator rights.

## Required behavior (MVP)

- The top-level window is always borderless.
- Left drag in the client area moves the window.
- Every edge and corner has a native resize hit target and cursor.
- Manual resize preserves the selected display aspect ratio.
- Right-click opens the complete command menu.
- Preview can be started and stopped. Stop releases the camera so other applications can use it.
- The user can select a camera and one of that camera's advertised resolutions.
- Display aspect choices are exactly `4:3`, `16:9`, and `Camera` (selected capture format).
- The image fills the client area without distortion. If source and display aspects differ, crop equally from the two long sides; do not letterbox.
- Horizontal and vertical mirror toggles can be combined.
- Always On Top is a toggle.
- Minimize and Exit are available from the context menu.
- Window placement and all user choices persist in `settings.ini` next to the executable.
- Missing cameras, unsupported saved modes, malformed settings, and disconnected devices fail safely and keep the menu usable.

## Context-menu contract

The initial implementation uses this order and these labels:

1. `Start Preview` or `Stop Preview` (dynamic label)
2. `Aspect Ratio` submenu: `4:3`, `16:9`, `Camera`
3. `Camera` submenu: discovered device names
4. `Resolution` submenu: unique `WIDTH x HEIGHT` modes for the selected camera
5. separator
6. `Mirror Horizontally`
7. `Mirror Vertically`
8. `Always On Top`
9. separator
10. `Minimize`
11. `Exit`

Check marks show the current aspect, camera, resolution, mirror flags, and topmost state. Disable camera-dependent commands when no usable camera exists.

## Deliberate non-goals for MVP

- Recording, snapshots, microphone/audio, virtual camera output, hotkeys, opacity, ellipse/rounded/full-screen modes, clipboard features, digital zoom, installer, autostart, and localization.
- Pixel-perfect cloning of undocumented Delphi internals.
- Copying code from `tCamView`; it is only a behavioral reference and carries GPL-related files.
- Device-specific camera-control UI. A standard DirectShow property page can be added after the MVP if requested.

## Acceptance summary

The MVP is complete only when a Release x64 build runs from a clean folder containing only the EXE and INI, performs every required menu action, restarts with restored state, and passes the manual matrix in `implementationPlan.md` on the target Windows versions.
