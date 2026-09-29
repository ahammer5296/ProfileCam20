# System Patterns

## Fixed implementation decision

Use native C++17, Unicode Win32, DirectShow, the system Sample Grabber filter, and GDI rendering.

- Build `Release|x64` first and statically link the runtime (`/MT`). Add Win32 only if target testing finds a device/filter that requires it.
- Link only Windows libraries: `strmiids.lib`, `quartz.lib`, `ole32.lib`, `oleaut32.lib`, `uuid.lib`, `gdi32.lib`, `user32.lib`, `comctl32.lib`, and `shlwapi.lib` if needed for path handling.
- Do not add NuGet, vcpkg, OpenCV, AForge, WIL, ATL, MFC, Qt, or a bundled .NET runtime.
- Declare the deprecated `ISampleGrabber` and `ISampleGrabberCB` interfaces locally because current SDK headers omit `qedit.h`. Keep these declarations isolated in `directshow_compat.h`.
- This decision may be revisited only if the Sample Grabber is absent or incompatible on an actual target LTSC machine. Do not switch stacks merely because DirectShow is deprecated.

## File boundaries

- `src/ProfileCam20/main.cpp`: `wWinMain`, COM initialization, message loop, top-level error boundary.
- `src/ProfileCam20/app_window.h/.cpp`: window class, commands/menu, state transitions, sizing/moving, painting, device-change handling.
- `src/ProfileCam20/camera.h/.cpp`: DirectShow enumeration, capability enumeration, graph lifecycle, sample callback, owned latest-frame buffer.
- `src/ProfileCam20/directshow_compat.h`: Sample Grabber COM declarations and CLSIDs only.
- `src/ProfileCam20/settings.h/.cpp`: typed settings model, INI read/write, validation helpers, executable-directory path.
- `src/ProfileCam20/resource.h`, `ProfileCam20.rc`, and `app.manifest`: icon/version/manifest and DPI declarations.

Keep UI code independent of raw COM details. Keep settings code independent of HWND and DirectShow objects.

## Ownership and threading

- Initialize COM as STA on the UI thread with `CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)`.
- Own and release every COM interface with a tiny local RAII pointer type or explicit, auditable `Release` calls. Do not mix ownership conventions.
- Build, run, stop, and destroy the filter graph on the UI thread.
- DirectShow invokes `ISampleGrabberCB::SampleCB` on a worker thread. The callback must only copy sample bytes and metadata into an owned buffer under a short lock, set an atomic notification flag, and `PostMessage(WM_APP_FRAME_READY)`. It must never paint, touch menus, rebuild the graph, or retain the `IMediaSample` pointer.
- Coalesce frame notifications so the message queue cannot grow by one message per frame.
- During shutdown: mark capture as stopping, stop graph, detach callback, release graph/interfaces, then destroy the window and uninitialize COM.

## Capture graph

1. Enumerate `CLSID_VideoInputDeviceCategory` with `ICreateDevEnum` and `IEnumMoniker`.
2. Store both friendly name and moniker display name. The display name is the stable settings key; the friendly name is UI text.
3. Bind the selected moniker to `IBaseFilter`.
4. Use `ICaptureGraphBuilder2::FindInterface` to obtain `IAMStreamConfig` on the capture pin.
5. Enumerate `GetStreamCaps`, accept video formats with valid positive width and absolute height, and deduplicate UI entries by width/height. Retain the full `AM_MEDIA_TYPE` for the chosen entry and free all media-type allocations correctly.
6. Set the chosen format before rendering the stream.
7. Add Sample Grabber and Null Renderer filters. Request `MEDIATYPE_Video`, `MEDIASUBTYPE_RGB32`, and `FORMAT_VideoInfo`; allow DirectShow intelligent connect to insert a converter from MJPG/YUY2 when necessary.
8. Connect with `RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, source, grabber, nullRenderer)`. If no preview pin exists, retry with `PIN_CATEGORY_CAPTURE`.
9. After connection, read the connected media type and record width, absolute height, stride, and top-down/bottom-up orientation. Reject unsupported layouts with a user-visible error while keeping the app alive.
10. `Run` starts preview. Stop destroys the graph and releases the device so another process can open it.

Changing camera or resolution remembers whether preview was running, tears down the old graph, updates selection, and restarts only if it was previously running. On failure, keep the new selection visible, set state to stopped, and show one concise error.

## Window behavior

- Create a `WS_POPUP` top-level window; never toggle to a captioned/thick-frame style.
- Handle `WM_NCHITTEST` with a DPI-scaled 8 px logical grip and return all edge/corner hit-test codes. Interior client space remains draggable.
- On left-button down in the interior, call `ReleaseCapture` and send `WM_NCLBUTTONDOWN/HTCAPTION`.
- Handle `WM_SIZING`, not `WM_SIZE`, to preserve aspect during interactive resize without recursion or jitter. Anchor the opposite edge/corner indicated by `wParam`.
- Use a minimum client size of 160 x 90 logical pixels, adjusted for the selected ratio.
- Aspect ratio is display width/display height: exactly `4.0/3.0`, `16.0/9.0`, or current connected/selected camera width divided by height. If Camera mode has no valid format, fall back to 4:3.
- Use per-monitor-v2 DPI awareness in the manifest. On `WM_DPICHANGED`, accept the suggested rectangle and recompute grip/minimum sizes.
- On restore, validate the saved rectangle against `MonitorFromRect`; if it is completely off-screen, center the default-sized window on the primary work area.

## Rendering behavior

- Keep one owned latest RGB32 frame. Older frames may be dropped; preview latency matters more than displaying every frame.
- `WM_PAINT` snapshots the latest buffer/metadata under the frame lock, then releases the lock before drawing.
- Compute a centered source crop whose aspect equals the client aspect. Crop left/right when source is wider; crop top/bottom when source is taller.
- Draw the crop to the full client rectangle with `SetStretchBltMode(HALFTONE)` and `StretchDIBits`.
- Implement mirroring by reversing destination axes (adjust origin and pass a negative destination width and/or height); account explicitly for the DIB's stored orientation. Test all four mirror combinations with an asymmetric scene.
- Paint black plus a short status string when stopped, no camera is present, or no frame has arrived. Do not show repeated modal errors from the frame path.

## Settings

Use Win32 profile APIs and save `settings.ini` beside the executable, never in the current working directory.

Canonical schema:

```ini
[window]
left=100
top=100
width=640
height=480
always_on_top=1

[preview]
device_id=@device:pnp:...
device_name=HD Pro Webcam C920
width=1280
height=720
aspect=camera
mirror_horizontal=0
mirror_vertical=0
running=1
```

- Also read the legacy `[position] left/top/width/height` and `[device] devicename` keys when canonical keys are absent.
- Resolve a saved camera by `device_id`, then unique friendly name, then first available device.
- Resolve resolution by exact width/height, then the largest format with the same aspect, then the largest available format.
- Clamp booleans and dimensions; unknown aspect values become `camera`.
- Save after each menu state change and on `WM_EXITSIZEMOVE`; write final state again during orderly shutdown.
- Failure to read or write settings is nonfatal.

## State model

Use explicit states: `Stopped`, `Starting`, `Running`, `Stopping`, and `Error`. Menu label and enablement derive from this state; do not infer running state from a non-null pointer. Reentrant Start/Stop and selection commands are ignored while Starting or Stopping.

## Error policy

- Convert failed `HRESULT`s to messages containing the operation name and hexadecimal code.
- Expected device absence/contention is presented once and leaves the window/menu responsive.
- No camera at startup is not a fatal application error.
- Never use sleeps or busy `Application.DoEvents`-style loops on the UI thread.
