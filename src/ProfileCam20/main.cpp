#include <windows.h>
#include <objbase.h>
#include "app_window.h"
#include "diagnostics.h"
#include "settings.h"

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE, PWSTR commandLine,int){ const bool commandLineDiagnostic=commandLine&&wcsstr(commandLine,L"--diagnostic")!=nullptr; const bool settingsDiagnostic=GetPrivateProfileIntW(L"diagnostics",L"enabled",0,SettingsPath().c_str())!=0; ConfigureDiagnosticLogging(commandLineDiagnostic||settingsDiagnostic); InstallCrashDiagnostics(); ClearDiagnosticLog(); DiagnosticLog(L"Application start"); SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); if(FAILED(hr)){DiagnosticLogHRESULT(L"CoInitializeEx",hr);return static_cast<int>(hr);} DiagnosticLog(L"COM STA initialized"); AppWindow app; int result=0; if(!app.Create(instance)){DiagnosticLog(L"AppWindow::Create failed");CoUninitialize();return 1;} MSG msg; while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);} DiagnosticLog(L"Application shutdown"); CoUninitialize(); return result; }
