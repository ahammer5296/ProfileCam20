# Implementation Plan

This document is deliberately procedural. A smaller coding model should execute one phase at a time, build after each phase, and avoid opportunistic refactors.

## Global rules for every phase

1. Work only under `D:\Dev\Camera\ProfileCam20`; leave the legacy EXE/INI and `D:\Dev\Camera\tCamView` untouched.
2. Before editing, reread `activeContext.md`, `projectbrief.md`, and `systemPatterns.md`.
3. Use warnings level 4, Unicode, C++17, exceptions disabled only if the code is written accordingly, and `/permissive-`.
4. Keep the application usable when no camera is connected.
5. After a successful build, record the exact command and result in `progress.md`.
6. Do not begin a later phase while the current phase has compile errors or failed checks.

Use this build command (single PowerShell line):

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\ProfileCam20.sln /m /p:Configuration=Release /p:Platform=x64
```

## Phase 0 - Native project skeleton

Deliverables:

- Create `ProfileCam20.sln` and `src\ProfileCam20\ProfileCam20.vcxproj` for `Release|x64` and `Debug|x64`.
- Add the files listed in `systemPatterns.md`; initially keep camera/settings implementations as compiling stubs.
- Configure Unicode, C++17, Windows subsystem, `/W4`, `/permissive-`, static CRT (`/MT` Release, `/MTd` Debug), and the system libraries listed in `systemPatterns.md`.
- Add a per-monitor-v2 DPI-aware manifest and a basic version resource.
- `wWinMain` must initialize COM STA, register the window class, create a black 640x480 `WS_POPUP` window, run the message loop, and uninitialize COM.
- Implement right-click menu with required static items, Exit, Minimize, and Always On Top. Camera-related items may be disabled stubs.

Verification:

- Release x64 builds with zero errors.
- Dependency inspection shows no AForge/OpenCV/.NET DLLs.
- Window is borderless, menu opens, topmost toggles, minimize works, and Exit terminates cleanly.

## Phase 1 - Window interaction and aspect constraints

Deliverables:

- Add typed UI state for aspect mode and topmost.
- Implement DPI-scaled edge/corner `WM_NCHITTEST` before interior drag handling.
- Implement interior left-drag using native caption movement.
- Implement `WM_SIZING` for 4:3, 16:9, and Camera fallback 4:3. Preserve the opposite edge/corner and enforce minimum size.
- Change aspect menu checks and immediately adjust current height to the chosen ratio while retaining width where possible.
- Handle `WM_DPICHANGED`.

Verification matrix:

- Resize from each of 4 edges and 4 corners; ratio error after release is at most one pixel.
- Cursor matches the resize direction.
- Dragging more than 8 logical px from every edge moves rather than resizes.
- Switching 4:3/16:9 changes window shape and checked item.
- Behavior remains usable at 100%, 125%, 150%, and 200% scale.

## Phase 2 - Settings and safe restoration

Deliverables:

- Implement the canonical INI schema and legacy-key fallback from `systemPatterns.md`.
- Obtain INI path from the executable path with `GetModuleFileNameW`.
- Restore size/position/aspect/mirrors/topmost/running intent before showing the window.
- Validate the restored rectangle against monitor work areas.
- Save UI changes immediately and placement on `WM_EXITSIZEMOVE`/shutdown.
- Keep both `device_id` and friendly `device_name` fields, even though enumeration is not implemented until Phase 3.

Verification:

- Start with no INI, malformed integers, invalid booleans, unknown aspect, tiny/huge sizes, and an off-screen rectangle.
- Start using the provided legacy INI and confirm its position/device-name values are recognized.
- Run from a working directory different from the EXE directory and confirm the adjacent INI is used.

## Phase 3 - Device and capability enumeration

Deliverables:

- Implement `directshow_compat.h` and COM/media-type cleanup helpers.
- Enumerate video-input monikers into `{friendlyName, displayName}` records.
- Enumerate `IAMStreamConfig` capabilities for the selected source.
- Validate formats, use absolute height, retain frame rate/subtype for deterministic sorting, and deduplicate the visible resolution list by dimensions.
- Deterministic order: descending pixel count, then width, then height; for duplicate dimensions prefer RGB32, then YUY2, then MJPG, then highest FPS.
- Resolve saved selections using the fallback policy in `systemPatterns.md`.
- Rebuild Camera and Resolution submenus and check the selected entries.
- Refresh device list on startup and relevant `WM_DEVICECHANGE` notifications; do not auto-start yet.

Verification:

- No-camera startup retains a responsive menu and disabled camera-dependent commands.
- C525/C920 names and common modes appear once per dimension.
- Selecting a camera rebuilds its resolution submenu without leaks/crashes.
- Repeated refreshes and camera unplug/replug do not leave stale checked indices.

## Phase 4 - Preview graph and frame handoff

Deliverables:

- Implement the state machine exactly as documented.
- Build the selected DirectShow graph and set the selected `AM_MEDIA_TYPE` before connection.
- Connect source -> Sample Grabber -> Null Renderer, requesting RGB32; retry capture pin when preview pin is absent.
- Implement a reference-counted `ISampleGrabberCB` callback and owned latest-frame buffer.
- Post coalesced `WM_APP_FRAME_READY`; invalidate and clear the notification flag on the UI thread.
- Implement Start/Stop dynamic label. Stop must destroy the graph and release the device.
- Restore `running=1` only after the window and selections are valid.
- Surface one readable error for contention or graph failure and return to Stopped/Error without exiting.

Verification:

- Start displays advancing frames; Stop turns the camera LED off and allows another camera application to open it.
- At least 20 Start/Stop cycles succeed.
- Changing camera/resolution while running stops/rebuilds/restarts exactly once.
- Closing while running exits promptly with no access violation or hung process.
- Opening the camera concurrently elsewhere produces a single error and leaves controls usable.

## Phase 5 - Crop, paint, and mirrors

Deliverables:

- Implement `WM_PAINT` latest-frame snapshotting and black/status fallback.
- Implement centered fill crop for arbitrary source/client ratios.
- Draw through `StretchDIBits` and explicitly handle connected top-down/bottom-up orientation.
- Add horizontal and vertical mirror toggles; save immediately and repaint without rebuilding the graph.
- If Camera aspect is active and a format changes, update the sizing ratio and normalize current window height.

Verification matrix:

- Source 16:9 -> display 4:3 crops equal left/right portions with no stretch.
- Source 4:3 -> display 16:9 crops equal top/bottom portions with no stretch.
- Camera aspect shows the full source modulo rounding.
- Test none, horizontal, vertical, and both mirrors using text or another asymmetric target.
- Resize continuously during preview; there are no crashes, obvious tearing caused by shared-buffer mutation, or unbounded memory growth.

## Phase 6 - Robustness and packaging

Deliverables:

- Audit every `AM_MEDIA_TYPE`, `CoTaskMemAlloc` result, COM pointer, GDI object, lock, and callback reference.
- Add concise HRESULT formatting and suppress duplicate repeated errors.
- Ensure menu commands are disabled during Starting/Stopping.
- Produce Release x64 into `dist\ProfileCam20\` with only `ProfileCam20.exe` and a documented/default `settings.ini` if desired.
- Do not copy `qedit.dll`, `quartz.dll`, VC runtime DLLs, or any file from `tCamView`.

Verification:

- Build from a clean solution state.
- Run for at least 30 minutes while resizing and toggling mirrors.
- Check handles and private bytes remain broadly stable across 50 graph rebuilds.
- Run the packaged folder on LTSC 2019 and LTSC 2021 with C525 and C920.
- Test no camera, busy camera, unplug while running, replug, multi-monitor negative coordinates, restart after forced termination, and read-only INI directory.

## Definition of done

- Every required behavior in `projectbrief.md` is manually verified.
- No unresolved build warning indicates truncation, unsafe COM ownership, or architecture mismatch.
- `progress.md` records build command, tested OS/device combinations, known limitations, and final artifact path.
- Any failed target-machine test is documented with exact HRESULT and step; architecture changes require evidence from that failure.
