#include "settings.h"
#include <shlwapi.h>
#include <algorithm>

namespace {
std::wstring ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* fallback, const std::wstring& path) { wchar_t value[1024]{}; GetPrivateProfileStringW(section,key,fallback,value,ARRAYSIZE(value),path.c_str()); return value; }
int ReadInt(const wchar_t* section, const wchar_t* key, int fallback, const std::wstring& path) { return static_cast<int>(GetPrivateProfileIntW(section,key,fallback,path.c_str())); }
bool HasKey(const wchar_t* section, const wchar_t* key, const std::wstring& path) { wchar_t value[4]{}; return GetPrivateProfileStringW(section,key,L"",value,ARRAYSIZE(value),path.c_str()) != 0; }
int ClampDimension(int value, int fallback, int minimum, int maximum) { if (value < minimum || value > maximum) return fallback; return value; }
std::wstring CameraSection(const std::wstring& deviceId) {
    unsigned long long hash=1469598103934665603ull;
    for(wchar_t c:deviceId){hash^=static_cast<unsigned long long>(c);hash*=1099511628211ull;}
    wchar_t section[64]{}; swprintf_s(section,L"camera_%016llX",hash); return section;
}
void RestoreVisible(RECT& r) {
    HMONITOR monitor=MonitorFromRect(&r,MONITOR_DEFAULTTONULL); MONITORINFO mi{sizeof(mi)};
    if (monitor && GetMonitorInfoW(monitor,&mi)) { RECT intersection{}; if (IntersectRect(&intersection,&r,&mi.rcWork)) return; }
    monitor=MonitorFromWindow(GetDesktopWindow(),MONITOR_DEFAULTTOPRIMARY); if (!GetMonitorInfoW(monitor,&mi)) return;
    LONG width=r.right-r.left, height=r.bottom-r.top; r.left=mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-width)/2; r.top=mi.rcWork.top+(mi.rcWork.bottom-mi.rcWork.top-height)/2; r.right=r.left+width; r.bottom=r.top+height;
}
}

std::wstring SettingsPath() { wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr,path,MAX_PATH); PathRemoveFileSpecW(path); return std::wstring(path)+L"\\settings.ini"; }

AppSettings LoadSettings() {
    AppSettings s; const std::wstring path=SettingsPath();
    const bool canonicalLeft=HasKey(L"window",L"left",path), canonicalTop=HasKey(L"window",L"top",path), canonicalWidth=HasKey(L"window",L"width",path), canonicalHeight=HasKey(L"window",L"height",path);
    s.window.left=ReadInt(L"window",L"left",canonicalLeft?100:ReadInt(L"position",L"left",100,path),path);
    s.window.top=ReadInt(L"window",L"top",canonicalTop?100:ReadInt(L"position",L"top",100,path),path);
    const int width=ReadInt(L"window",L"width",canonicalWidth?640:ReadInt(L"position",L"width",640,path),path);
    const int height=ReadInt(L"window",L"height",canonicalHeight?480:ReadInt(L"position",L"height",480,path),path);
    s.window.right=s.window.left+ClampDimension(width,640,160,7680); s.window.bottom=s.window.top+ClampDimension(height,480,90,4320); RestoreVisible(s.window);
    s.alwaysOnTop=ReadInt(L"window",L"always_on_top",0,path)!=0;
    std::wstring aspect=ReadString(L"preview",L"aspect",L"camera",path); if (!_wcsicmp(aspect.c_str(),L"4:3")) s.aspect=AspectMode::Ratio43; else if (!_wcsicmp(aspect.c_str(),L"16:9")) s.aspect=AspectMode::Ratio169;
    s.mirrorHorizontal=ReadInt(L"preview",L"mirror_horizontal",0,path)!=0; s.mirrorVertical=ReadInt(L"preview",L"mirror_vertical",0,path)!=0; s.runningIntent=ReadInt(L"preview",L"running",0,path)!=0;
    s.deviceId=ReadString(L"preview",L"device_id",L"",path); s.deviceName=ReadString(L"preview",L"device_name",L"",path); s.width=ClampDimension(ReadInt(L"preview",L"width",640,path),640,1,7680); s.height=ClampDimension(ReadInt(L"preview",L"height",480,path),480,1,4320);
    if (s.deviceName.empty()) s.deviceName=ReadString(L"device",L"devicename",L"",path);
    return s;
}

void SaveSettings(const AppSettings& s) {
    const std::wstring path=SettingsPath(); wchar_t b[32]{};
    wsprintfW(b,L"%ld",s.window.left); WritePrivateProfileStringW(L"window",L"left",b,path.c_str()); wsprintfW(b,L"%ld",s.window.top); WritePrivateProfileStringW(L"window",L"top",b,path.c_str()); wsprintfW(b,L"%ld",s.window.right-s.window.left); WritePrivateProfileStringW(L"window",L"width",b,path.c_str()); wsprintfW(b,L"%ld",s.window.bottom-s.window.top); WritePrivateProfileStringW(L"window",L"height",b,path.c_str()); WritePrivateProfileStringW(L"window",L"always_on_top",s.alwaysOnTop?L"1":L"0",path.c_str());
    const wchar_t* aspect=s.aspect==AspectMode::Ratio43?L"4:3":(s.aspect==AspectMode::Ratio169?L"16:9":L"camera"); WritePrivateProfileStringW(L"preview",L"aspect",aspect,path.c_str()); WritePrivateProfileStringW(L"preview",L"mirror_horizontal",s.mirrorHorizontal?L"1":L"0",path.c_str()); WritePrivateProfileStringW(L"preview",L"mirror_vertical",s.mirrorVertical?L"1":L"0",path.c_str()); WritePrivateProfileStringW(L"preview",L"running",s.runningIntent?L"1":L"0",path.c_str()); WritePrivateProfileStringW(L"preview",L"device_id",s.deviceId.c_str(),path.c_str()); WritePrivateProfileStringW(L"preview",L"device_name",s.deviceName.c_str(),path.c_str()); wsprintfW(b,L"%d",s.width); WritePrivateProfileStringW(L"preview",L"width",b,path.c_str()); wsprintfW(b,L"%d",s.height); WritePrivateProfileStringW(L"preview",L"height",b,path.c_str());
}

bool LoadCameraResolution(const std::wstring& deviceId, int fallbackWidth, int fallbackHeight, int& width, int& height) {
    width=fallbackWidth; height=fallbackHeight; if(deviceId.empty())return false;
    const std::wstring path=SettingsPath(), section=CameraSection(deviceId);
    const std::wstring storedId=ReadString(section.c_str(),L"device_id",L"",path);
    if(!storedId.empty()&&storedId!=deviceId)return false;
    const int storedWidth=ReadInt(section.c_str(),L"width",fallbackWidth,path), storedHeight=ReadInt(section.c_str(),L"height",fallbackHeight,path);
    if(storedWidth<1||storedWidth>7680||storedHeight<1||storedHeight>4320)return false;
    width=storedWidth;height=storedHeight;return true;
}

void SaveCameraResolution(const std::wstring& deviceId, const std::wstring& deviceName, int width, int height) {
    if(deviceId.empty()||width<1||height<1)return;
    const std::wstring path=SettingsPath(), section=CameraSection(deviceId); wchar_t value[32]{};
    WritePrivateProfileStringW(section.c_str(),L"device_id",deviceId.c_str(),path.c_str());
    WritePrivateProfileStringW(section.c_str(),L"device_name",deviceName.c_str(),path.c_str());
    wsprintfW(value,L"%d",width); WritePrivateProfileStringW(section.c_str(),L"width",value,path.c_str());
    wsprintfW(value,L"%d",height); WritePrivateProfileStringW(section.c_str(),L"height",value,path.c_str());
}
