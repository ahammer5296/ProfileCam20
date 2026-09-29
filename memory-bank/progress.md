# Progress

## Completed

- Cloned `augamvio/tCamView` to `D:\Dev\Camera\tCamView`.
- Reviewed the WinForms implementation and identified its current camera, menu, borderless, aspect-fill, and window-drag logic.
- Implemented requested changes in `D:\Dev\Camera\tCamView\Form1.cs`: borderless-only styles, native borderless resize hit-testing, proportional fill behavior, and first menu item `Start/Stop View`.
- Static checks of that reference project were completed. The earlier statement that MSBuild was unavailable was incorrect: Visual Studio/MSVC/MSBuild are installed, but no .NET SDK is installed. The selected replacement does not need the .NET SDK.
- Inspected the legacy `ProfileCam.exe` and `settings.ini`; findings are recorded in `activeContext.md`.
- Parsed the legacy PE import table and confirmed that `vcltest3.dll` is not an import.
- Confirmed the legacy aspect choices from embedded form data: 4:3, 16:9, and camera-derived.
- Fixed the new implementation stack: native C++17/Win32 + DirectShow Sample Grabber + GDI, static CRT, no third-party runtime.
- Expanded the product contract, component boundaries, state model, settings schema, error policy, test matrix, and implementation sequence.

## Next steps

## Phase 0 completed

- Created `ProfileCam20.sln` and `src\ProfileCam20\ProfileCam20.vcxproj` for Debug/Release x64.
- Added the native Win32 source boundaries, DirectShow compatibility placeholder, settings placeholder, version resource, and PerMonitorV2 manifest.
- Implemented COM STA startup, borderless black window, right-click menu skeleton, topmost toggle, minimize, exit, and adjacent-INI placement persistence stub.
- Build command: `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\ProfileCam20.sln /m /p:Configuration=Release /p:Platform=x64`
- Result: successful Release x64 build, 0 errors, 0 compiler warnings. Artifact: `dist\ProfileCam20\Release\ProfileCam20.exe`.
- `dumpbin /DEPENDENTS` shows only system dependencies (`ole32.dll`, `GDI32.dll`, `USER32.dll`, `SHLWAPI.dll`, `KERNEL32.dll`); no .NET, AForge, or OpenCV runtime.
- Manifest authoring is warning-free; runtime explicitly requests `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2` before window creation.
- Final clean verification: `MSBuild.exe .\ProfileCam20.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64` — 0 errors, 0 warnings.

The next agent should start at Phase 1 in `implementationPlan.md` after fixing the manifest warning, and stop after each phase to build and record verification. Do not edit `D:\Dev\Camera\tCamView` further unless explicitly asked.

## Phase 1 partial completion

- Added typed `AspectMode` with persisted `4:3`, `16:9`, and `camera` values.
- Added checked Aspect Ratio submenu items and immediate window ratio adjustment.
- Added DPI-scaled 8 logical-pixel edge/corner hit testing, `WM_SIZING` ratio enforcement, minimum size, `WM_DPICHANGED`, and placement save on `WM_EXITSIZEMOVE`.
- Added persisted mirror toggle state and checked menu entries; actual frame mirroring remains Phase 5.
- Build command: `MSBuild.exe .\ProfileCam20.sln /m /p:Configuration=Release /p:Platform=x64` — successful, 0 errors, 0 warnings.
- Manual resize verification across DPI scales is still pending; camera mode currently falls back to 4:3 until Phase 3 capability data exists.

## Phase 2 completed

- Implemented canonical window/preview INI fields, executable-adjacent path resolution, and legacy `[position]`/`[device]` fallback.
- Persisted and restored aspect, mirror flags, running intent, device id/name, and selected width/height.
- Added dimension clamping and off-screen rectangle recovery to the primary monitor work area.
- Release x64 build succeeded with 0 errors and 0 warnings using the prescribed MSBuild command.
- Real-device and malformed-settings manual matrix remains pending; camera selection fields are ready for Phase 3 enumeration.

## Phase 3 implementation

- Added DirectShow video-input enumeration with friendly and stable display names.
- Added `IAMStreamConfig::GetStreamCaps` parsing for VideoInfo/VideoInfo2, absolute dimensions, frame rate, subtype, and owned media types.
- Added deterministic capability selection: deduplicate by dimensions, prefer RGB32/YUY2/MJPG, then FPS; sort by pixels, width, height.
- Added dynamic Camera and Resolution submenus, checked selected entries, persisted selection, and refresh on device arrival/removal.
- No-camera state remains usable with disabled placeholder entries.
- Release x64 build succeeded with 0 errors and 0 warnings using the prescribed MSBuild command.
- Hardware verification with C525/C920 and unplug/replug behavior remains pending.
- Added a read-only Diagnostics submenu showing each detected camera and its discovered mode count.
- Startup selection now follows `device_id`, then unique friendly name, then first available camera; selected device/mode is persisted.
- Automatic preview activation is intentionally deferred to Phase 4 because the DirectShow graph/start state machine is not implemented yet.

## Phase 4 implementation

- Added local `ISampleGrabber`/`ISampleGrabberCB` declarations and a DirectShow graph path: source → Sample Grabber → Null Renderer.
- Added reference-counted callback, owned latest RGB32 frame buffer, coalesced frame notification, Start/Stop graph lifecycle, and frame painting.
- Start/Stop menu label is dynamic; changing camera or resolution rebuilds and restarts when previously running.
- Startup automatically starts preview when a selected camera has a usable discovered format; failed graph startup leaves the menu usable with a status message.
- Release x64 build succeeded with 0 errors and 0 warnings.
- Real camera runtime validation, crop correctness, connected media orientation, and full mirror matrix remain pending.
- Added graph-failure diagnostics with operation name and hexadecimal HRESULT rendered in the preview status area.
- Fixed capability enumeration by adding the bound source filter to the temporary graph before `FindInterface`; previously devices could appear with `0 modes`, causing `No usable camera`.
- Enabled the Diagnostics popup; it had been accidentally created with `MF_GRAYED`.
- Added UTF-8 `ProfileCam20.log` beside the executable. Each run records startup/COM state and failed preview graph stages with HRESULT values; the log is reset at process start.
- Expanded the log with successful capability enumeration, graph/filter creation, callback setup, stream connection, and graph-running milestones.
- Fixed preview bind after log analysis: both capabilities and graph startup now use the original DirectShow `IMoniker` path instead of reparsing `displayName` with `MkParseDisplayName` (`0x80070057`).

## Open validation risks (not architecture questions)

- Verify that Sample Grabber and RGB32 intelligent conversion work on LTSC 2019 and LTSC 2021 with both C525 and C920.
- Determine whether multiple advertised formats share dimensions but differ in FPS/subtype. MVP deliberately exposes one deterministic choice per dimension; revisit only if testing shows a practical problem.
- Confirm the GDI sign/orientation math for all four mirror combinations with top-down and bottom-up connected media types.
- Confirm USB disconnect/reconnect behavior and `WM_DEVICECHANGE` refresh on real hardware.
- Confirm that `/MT` Release output starts on clean LTSC machines without VC++ redistributable installation.

## Current blocker / continuation point

- The latest Release x64 build completed successfully after switching the Sample Grabber callback from `SampleCB` (method 1) to `BufferCB` (method 0): 0 errors, 0 warnings.
- The latest reproduced runtime failure was a real process crash. Log time: `2026-09-30 01:15:49.934`; exception: access violation `0xC0000005` at `00007FFEE99D3AA8`. The graph had reached `Camera graph running`, but no first sample and no `Application shutdown` were logged. The UI remained on `Starting preview` and then closed after a delay.
- Before the crash, the log confirmed that the C525 filter binds successfully and the stream connects. The camera works in other applications; DirectShow discovers it and reports 25 usable modes. Earlier `MkParseDisplayName` failures with `0x80070057` were fixed by binding through the original `IMoniker`.
- The current mitigation is the `BufferCB` callback path with a first-buffer diagnostic and an unhandled-exception logger. This build has not yet been validated by a new user run, so the crash is not considered fixed.
- Next step: run the newest `dist\\ProfileCam20\\Release\\ProfileCam20.exe` and inspect `ProfileCam20.log` for `Buffer callback connected`, `First buffer received`, `Camera graph running`, or another `Unhandled exception`. If the crash remains, isolate callback/frame-buffer access from GDI painting and then record the connected media type/orientation.
- The Diagnostics menu is enabled and reports device names plus mode counts. `OBS Virtual Camera` may legitimately report no `IAMStreamConfig`; this is logged as `0x80004002` and is not the C525 crash.

## 2026-09-30 follow-up

- Corrected `SetCallback` from method 0 (`SampleCB`) to method 1 (`BufferCB`). This was a confirmed API misuse; the first buffer now appears in the log.
- Checked capture `SetFormat` and the connected RGB32 media type; frame dimensions/orientation come from the connection instead of the requested format. Added teardown, periodic frame, and faulting-module diagnostics.
- Build: `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\ProfileCam20.sln /m /p:Configuration=Release /p:Platform=x64 /verbosity:minimal` — succeeded, 0 errors, 0 warnings after sandbox approval for Windows SDK directory access.
- Runtime: one intermittent `0xC0000005` in `ksproxy.ax` after first frame; later 25-second hidden captures at 1920x1080 and 640x480 stayed alive and logged advancing frames. Window visibility, clean Stop/Exit, and long-term stability remain unverified.

## 2026-09-30 camera-switch fix

- Reproduced the crash in a standalone C525 capture probe during graph teardown, independent of virtual cameras.
- Fixed a double `IMoniker::Release` in `BindDeviceFilter`. A control test with explicit filter removal but without this fix still crashed; after this fix, normal graph teardown completed cleanly.
- Passed 10 consecutive C525 start/stop cycles at 640x480, one cycle at the highest C525 mode, and one screen-capture-recorder start/stop cycle. Probe exit code was 0.
- Disabled cameras with no usable modes in the Camera menu and corrected selection to restart preview only if it was running. Added selection and per-interface teardown log entries.
- Build: `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\ProfileCam20.sln /m /p:Configuration=Release /p:Platform=x64 /verbosity:minimal` — succeeded, 0 errors, 0 warnings.
- Remaining: user-facing UI test of camera switching, long-duration stability, and virtual screen source image correctness. OBS Virtual Camera is unsupported by the current capability path (zero modes).

## 2026-09-30 flicker rendering change

- User reported black flashes during otherwise stable preview. Changed painting to reuse an offscreen GDI buffer and present completed frames with one `BitBlt`. Removed automatic background erase and black fill before each live frame.
- Added one-time diagnostic logging when `StretchDIBits` fails; the status screen still paints black when no live frame exists.
- Build: `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\ProfileCam20.sln /m /p:Configuration=Release /p:Platform=x64 /verbosity:minimal` — succeeded, 0 errors, 0 warnings.
- Remaining: visual test of flicker and mirror/crop behavior on the real window. The log alone cannot confirm whether black frames originate in the camera or the window renderer.

## 2026-09-30 frame rate and aspect fix

- Confirmed C525's high-resolution YUY2 modes advertise only 5–15 fps, while MJPG at the same dimensions advertises 30 fps. Changed duplicate-resolution choice to favor the higher frame rate, then subtype on ties.
- Candidate runtime at 1920x1080 connected MJPG with RGB32 output and 30 fps media type; observed buffer delivery was approximately 15 fps at 1920x1080, 1280x720, and 960x720. Capture-pin-first did not increase it. This is an improvement over the prior 5 fps at 1920x1080, though actual 30 fps is not established.
- Fixed Camera aspect to use selected capture width/height, normalized window height after startup and mode changes, and added centered source crop to avoid stretching when source/display ratios differ.
- Build: `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\ProfileCam20.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 /verbosity:minimal` — successful, 0 errors, 0 warnings. Final artifact: `dist\ProfileCam20\Release\ProfileCam20.exe`.
- Temporary format probe and candidate build files were removed. User's Release settings were preserved. Await visual confirmation of aspect and perceived frame rate; investigate camera exposure or driver pacing if 15 fps remains too slow.

## 2026-09-30 user confirmation

- User reports normal current operation with no observed issues. No further corrective work is requested at this time.

## 2026-09-30 diagnostic logging control

- Detailed `ProfileCam20.log` output is now disabled by default.
- It is enabled by `[diagnostics] enabled=1` in `settings.ini` or temporarily by launching `ProfileCam20.exe --diagnostic`.
- Unhandled-exception reports are written independently to `ProfileCam20.crash.log`.
- Release x64 rebuild succeeded with 0 errors and 0 warnings.

## 2026-09-30 application icon

- Extracted the legacy camera-lens icon from `ProfileCam.exe` and embedded it as the native app icon.
- Assigned the icon to the window class (`hIcon` and `hIconSm`) and rebuilt the Release x64 artifact successfully with 0 errors and 0 warnings.

## 2026-09-30 per-camera resolution history

- Added stable per-device resolution records in hashed `[camera_...]` INI sections.
- Camera switching and startup now restore the last exact resolution for that device; selecting a resolution updates its record.
- Release x64 rebuild succeeded with 0 errors and 0 warnings.

## 2026-09-30 menu cleanup

- Removed the `Diagnostics` submenu from the user-facing Camera menu while preserving diagnostic file logging controls.
- Release x64 rebuild succeeded with 0 errors and 0 warnings.
