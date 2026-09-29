#pragma once
#include <windows.h>

void ClearDiagnosticLog();
void ConfigureDiagnosticLogging(bool enabled);
bool DiagnosticLoggingEnabled();
void DiagnosticLog(const wchar_t* message);
void DiagnosticLogHRESULT(const wchar_t* operation, HRESULT hr);
void InstallCrashDiagnostics();
