# ProfileCam20

ProfileCam20 is a small portable Windows camera preview utility.

## Features

- Borderless, draggable and resizable preview window.
- DirectShow camera discovery and dynamic resolution selection.
- Display aspect modes: `4:3`, `16:9`, and `Camera`.
- Center-cropped preview without image distortion.
- Horizontal and vertical mirroring.
- Always-on-top mode.
- Per-camera resolution history.
- Portable settings stored next to the executable.
- Optional diagnostic logging.

## Requirements

- Windows 10 x64, including LTSC 2019 and LTSC 2021.
- A DirectShow-compatible USB/UVC camera.
- No installer, .NET runtime, or third-party runtime is required.

## Build

Open `ProfileCam20.sln` in Visual Studio 2022 and build `Release|x64`.

The portable output consists of:

```text
ProfileCam20.exe
settings.ini
```

## Diagnostics

Detailed runtime logging is disabled by default. Enable it in `settings.ini`:

```ini
[diagnostics]
enabled=1
```

For a single diagnostic run, start the application with:

```text
ProfileCam20.exe --diagnostic
```

Unhandled-exception information is written separately to `ProfileCam20.crash.log`.

## Project status

The current MVP is operational on the development machine. The project is intended for further validation on Windows 10 LTSC systems and additional camera models.
