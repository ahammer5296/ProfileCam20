#pragma once
#include <windows.h>
#include <string>

enum class AspectMode { Ratio43, Ratio169, Camera };
struct AppSettings { RECT window{100,100,740,580}; bool alwaysOnTop{}; AspectMode aspect{AspectMode::Camera}; bool mirrorHorizontal{}; bool mirrorVertical{}; bool runningIntent{}; std::wstring deviceId; std::wstring deviceName; int width{640}; int height{480}; };
std::wstring SettingsPath();
AppSettings LoadSettings();
void SaveSettings(const AppSettings& settings);
bool LoadCameraResolution(const std::wstring& deviceId, int fallbackWidth, int fallbackHeight, int& width, int& height);
void SaveCameraResolution(const std::wstring& deviceId, const std::wstring& deviceName, int width, int height);
