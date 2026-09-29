#pragma once
#include <windows.h>
#include "settings.h"
#include "camera.h"
#include <string>

class AppWindow { public: AppWindow()=default; ~AppWindow(); bool Create(HINSTANCE); HWND Handle() const { return hwnd_; } static LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM); private: HWND hwnd_{}; AppSettings settings_{}; CameraCatalog cameras_{}; CameraCapture capture_{}; size_t selectedDevice_{}; size_t selectedFormat_{}; bool running_{}; std::wstring status_; HDC paintBufferDc_{}; HBITMAP paintBufferBitmap_{}; HGDIOBJ paintBufferOldBitmap_{}; int paintBufferWidth_{}; int paintBufferHeight_{}; bool paintFailureLogged_{}; void ReleasePaintBuffer(); void ShowMenu(POINT); void UpdateTopmost(); void ChooseAspect(AspectMode); double Aspect() const; void EnforceSizing(RECT*, WPARAM) const; void ResolveSelection(); void SelectDevice(size_t); void SelectFormat(size_t); void RefreshDevices(); bool StartPreview(); void StopPreview(); void Paint(HDC); };
