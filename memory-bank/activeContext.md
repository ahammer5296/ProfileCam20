# Active Context

## Source artifacts

- Working directory: `D:\Dev\Camera\ProfileCam20`
- Legacy executable copy: `D:\Dev\Camera\ProfileCam20\ProfileCam.exe`
- Legacy settings copy: `D:\Dev\Camera\ProfileCam20\settings.ini`
- Existing C# reference project: `D:\Dev\Camera\tCamView`

The previously recorded directory `D:\Dev\ProfileCam20` does not exist on the current machine. Use the current working-directory copies.

## Legacy analysis findings

- `ProfileCam.exe` is a 32-bit PE (`IMAGE_FILE_MACHINE_I386`) with 8 sections.
- SHA-256: `57C69EC4E7FFEC3D7798C1953BEAE12F3C5F3ECCB8D379247C04B5B0346A5EA7`.
- It contains Delphi/VCL runtime/type-information strings (`TForm`, `TButton`, `Forms`, `System`, `VCL`-style names).
- DirectShow is used: strings include `DirectShow9`, `ICaptureGraphBuilder2`, `IVideoWindow`, `IMediaEvent`, `IMediaFilter`, `VideoRenderer`, `VideoWindow`, and `AMMediaType`.
- The parsed PE import table contains only `advapi32.dll`, `comctl32.dll`, `gdi32.dll`, `kernel32.dll`, `ole32.dll`, `oleaut32.dll`, `olepro32.dll`, `quartz.dll`, `user32.dll`, and `version.dll`.
- `vcltest3.dll` is only an embedded string; it is not an imported DLL. Do not package it.
- UI/handler strings include `START Preview`, `STOP Preview`, `Video Options`, `Get Aspect Ratio From Camera`, `Device Settings`, `Preview Settings`, `VideoWindow1MouseDown`, `VideoWindow1MouseMove`, `VideoWindow1MouseUp`, `FormResize`, and `FormCanResize`.
- Embedded form data confirms the aspect options `Aspect Ratio 4:3`, `Aspect Ratio 16:9`, and `Get Aspect Ratio From Camera`.
- Embedded form data also contains the original misspelled caption `Cam Rezolution`; the replacement intentionally uses `Resolution`.
- The EXE has no useful version metadata, is unsigned, and does not contain recoverable source/debug information.
- The EXE file modification time was 2017-05-16; the internal PE timestamp is not trustworthy.

## Current machine/toolchain

- Visual Studio Community 2022 17.14 is installed at `C:\Program Files\Microsoft Visual Studio\2022\Community`.
- MSVC x86/x64 tools 14.38 and 14.44 and Windows SDK 10.0.26100.0 are installed.
- MSBuild is available at `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`; it is not currently on `PATH`.
- `dotnet` runtimes 7, 8, and 9 are installed, but there is no .NET SDK. This does not block the selected native C++ build.
- DirectShow Sample Grabber CLSID `{C1F400A0-3F08-11D3-9F0B-006008039E37}` and Null Renderer CLSID `{C1F400A4-3F08-11D3-9F0B-006008039E37}` are registered through `C:\Windows\System32\qedit.dll`. Both 64-bit and 32-bit `qedit.dll` files exist.
- The current `ProfileCam20` directory is not a Git repository. Preserve the legacy EXE and INI; create source in a separate `src` subtree.

## Legacy settings format

The current file contains:

```ini
[position]
left=1580
top=17
width=307
height=230
[device]
devicename=HD Webcam C525
```

Aspect values and the complete static import list are now confirmed. Runtime COM behavior still requires tests on the two target LTSC releases.

## Reference-project cautions

`D:\Dev\Camera\tCamView` is a .NET Framework 4.7.2 WinForms application using AForge 2.2.5. It demonstrates device enumeration, frame callbacks, menu state, mirroring, borderless hit-testing, and dragging, but it also contains many out-of-scope features and blocking waits on the UI thread. Treat it as behavioral evidence only; do not copy its implementation into the new native project.

## Current implementation/crash state

- The native replacement is at Phase 4: DirectShow graph, dynamic camera/resolution menus, automatic startup preview, persistence, diagnostics submenu, and detailed UTF-8 runtime logging are implemented.
- Camera discovery is working on the test machine: `HD Webcam C525` exposes 25 modes; the other discovered devices include `screen-capture-recorder` and `OBS Virtual Camera`. The C525 works in other programs.
- The confirmed current problem is a process access violation during automatic preview startup. The last log reached `Camera graph running` after successful filter bind and stream connection, then ended with `Unhandled exception: code=0xC0000005 address=00007FFEE99D3AA8`; no first frame callback or normal shutdown was logged. The visible sequence was `Starting preview`, a hang/delay, then application exit. Selecting another camera was not established as the cause.
- The latest source/build changes `ISampleGrabberCB` from `SampleCB` method 1 to `BufferCB` method 0, adds first-buffer logging and an unhandled-exception filter. The mitigation is compiled but requires a fresh runtime log before further conclusions.
- Continue from the newest executable and log. First determine whether `BufferCB` receives a frame. If it still crashes, temporarily separate callback copy/posting from `WM_PAINT` and capture the connected media type details. Do not re-investigate the already fixed `MkParseDisplayName` issue unless a new log shows it again.

## 2026-09-30 follow-up diagnosis

- Confirmed the prior callback change selected the wrong method: `ISampleGrabber::SetCallback(callback, 0)` invokes `SampleCB`; `1` invokes `BufferCB` per Microsoft documentation. The prior `BufferCB` implementation was never invoked.
- Fixed the callback index to `1`, checked `SetFormat`, and read/validated `GetConnectedMediaType` before graph execution. The connected RGB32 dimensions and orientation now drive buffer sizing. `running_` is atomic and enabled before `Run`, so early frames can be accepted.
- Expanded exception logging to include thread, faulting module/offset, and access type/address; added graph teardown checkpoints and a periodic frame count. The original crash log is preserved as `dist\ProfileCam20\Release\ProfileCam20.pre-fix.log`.
- A test after the callback fix still produced one access violation in `C:\Windows\System32\ksproxy.ax` reading `FFFFFFFFFFFFFFFF` after the first 1920x1080 frame. It was not tied to teardown; the attempted hidden-window close posted to a null handle.
- Subsequent hidden 25-second captures stayed alive and received 90 frames at 1920x1080/5 fps and 330 frames at 640x480/30 fps. The 640x480 test temporarily changed the distribution INI and restored its original bytes afterward. This is intermittent; no visual preview or clean Start/Stop verification is established yet.
- Release x64 rebuild succeeded after the diagnostic changes. An initial sandboxed rebuild failed with access denied to `C:\Users\Alex\AppData\Local\Microsoft SDKs`; the approved retry succeeded.
- Do not declare the crash fixed. Next reproduction should inspect whether it occurs during streaming or after `Camera graph stopping`, and use the module offset. A crash dump or debugger stack will be needed if the fault remains in `ksproxy.ax`.

## 2026-09-30 camera-switch diagnosis

- A user-triggered camera switch produced a log ending after `Camera graph stopped` and `Camera callback detached`, before any new device startup. The crash was therefore in teardown of the C525 graph, not in starting a virtual camera.
- A standalone capture probe reproduced `0xC0000005` while releasing the C525 filter graph at 640x480 without any UI or virtual camera selection.
- Root cause found in `BindDeviceFilter`: the matched `IMoniker` was released inside the match branch and then again after the loop. The double COM `Release` corrupted ownership and later failed during graph teardown. The function now releases each moniker once and returns `VFW_E_NOT_FOUND` if no match is found.
- Control experiment: adding explicit `RemoveFilter` calls before the fix still crashed. After correcting the double release, both with and without `RemoveFilter` completed cleanly; the extra filter-removal change was reverted.
- The probe passed 10 C525 start/stop cycles at 640x480, one start/stop at the highest C525 mode, and a switch to/start/stop of `screen-capture-recorder` without crashing. The virtual screen source's buffer length was larger than the connected RGB32 frame size; investigate preview correctness separately. OBS Virtual Camera has zero discoverable modes and is now disabled in the Camera menu.
- Selecting a camera now logs the target before teardown and only restarts preview if it was already running. Release x64 build succeeded after these changes. Full UI interaction and long-duration stability remain to be verified.

## 2026-09-30 black-frame flicker follow-up

- User confirmed camera-switch crashes disappeared, but preview visibly flickers with black frames.
- The latest log showed regular buffer callbacks for `screen-capture-recorder`, C525 at 1920x1080, and C525 at 160x120, plus clean graph teardown and application shutdown. It did not distinguish source-black frames from painting artifacts.
- The previous `WM_PAINT` filled the visible window black before every `StretchDIBits`. The window class also specified a black background brush. This can expose black during rendering.
- Added a reusable compatible memory DC/bitmap for offscreen painting and one final `BitBlt`; suppressed `WM_ERASEBKGND`, removed the class background brush, and stopped clearing black before a valid video frame. Black status remains when stopped or awaiting the first frame. A failed `StretchDIBits` is logged once until it recovers.
- Release x64 build succeeded. Visual flicker verification on the user's display is pending. If it persists, determine whether captured buffers themselves are black before replacing the render stack.
- A basic `IVideoWindow` child renderer is a possible alternative for simple unmodified preview, but this app also requires crop and mirror operations. Microsoft documents VMR windowless rendering as a more flexible DirectShow option for cropping and avoids some child-window issues.

## 2026-09-30 frame rate and Camera aspect follow-up

- User confirmed black flicker is gone. They then reported increasing slowdown above 960x720 and incorrect/distorted `Aspect Ratio -> Camera` behavior.
- Direct inspection of C525 capabilities showed YUY2 drops from 15 fps at 960x720 to 10 fps at 1280x720 and 5 fps at 1920x1080; the same dimensions are offered as MJPG at 30 fps. Previous dimension deduplication always preferred YUY2 over MJPG, causing the resolution-dependent slowdown.
- Format selection now prioritizes advertised fps, using subtype preference only to break fps ties. A format probe verified the selected C525 modes from 960x720 through 1920x1080 are MJPG 30 fps.
- Runtime candidate builds connected C525 at 1920x1080 MJPG/RGB32 30 fps and received roughly 15 buffers/s. The same observed ~15 buffers/s occurred at 1280x720 and 960x720 despite 30 fps negotiation. Capture-pin-first connection (with preview fallback) did not change that rate; process CPU was about 2.3 seconds over 6 seconds. Possible camera exposure/driver pacing remains unverified.
- `Aspect()` previously returned 4:3 for Camera mode. It now uses selected width/height; window shape is normalized at startup and on device/format changes. `WM_PAINT` now crops source centrally to the client aspect before stretching, avoiding distortion for 4:3/16:9 display modes.
- User's Release INI remains at C525 960x720 with `aspect=camera` and `running=0`; no active ProfileCam20 process remained at final build. The final Release x64 rebuild succeeded with zero errors and warnings. Visual aspect and perceived frame-rate verification in the real UI remains pending.

## 2026-09-30 current user status

- User reports that the application currently works normally; no issues or remarks have been observed so far.
- Treat the earlier pending visual-verification notes as historical until a new problem is reported.
- Detailed runtime logging is disabled by default. It can be enabled with `[diagnostics] enabled=1` in `settings.ini` or temporarily with the `--diagnostic` command-line argument. Crash reports remain independent in `ProfileCam20.crash.log`.
- The application icon was extracted from the legacy `ProfileCam.exe`, added as `ProfileCam20.ico`, embedded into the executable, and assigned to both the window class and small window icon.
- Resolution history is now stored per camera in hashed `[camera_...]` sections keyed by the stable DirectShow device ID. Switching cameras restores that camera's last exact width/height when the mode is still advertised.
- The `Diagnostics` submenu was removed from the user context menu; file logging remains available through the settings key and `--diagnostic` argument.
