#include "diagnostics.h"
#include <string>

namespace {
bool g_enabled = false;
std::wstring DirectoryPath() { wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr,path,MAX_PATH); wchar_t* slash=wcsrchr(path,L'\\'); if(slash)*slash=L'\0'; return path; }
std::wstring LogPath() { return DirectoryPath()+L"\\ProfileCam20.log"; }
std::wstring CrashLogPath() { return DirectoryPath()+L"\\ProfileCam20.crash.log"; }
void WriteLine(const std::wstring& path,const std::wstring& line) { HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr); if(file==INVALID_HANDLE_VALUE)return; SYSTEMTIME st{};GetLocalTime(&st); wchar_t prefix[64];wsprintfW(prefix,L"[%04u-%02u-%02u %02u:%02u:%02u.%03u] ",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,st.wMilliseconds);std::wstring text=prefix+line+L"\r\n";int bytes=WideCharToMultiByte(CP_UTF8,0,text.c_str(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);if(bytes>0){std::string utf8(static_cast<size_t>(bytes),'\0');WideCharToMultiByte(CP_UTF8,0,text.c_str(),static_cast<int>(text.size()),utf8.data(),bytes,nullptr,nullptr);DWORD written=0;WriteFile(file,utf8.data(),static_cast<DWORD>(utf8.size()),&written,nullptr);}CloseHandle(file); }
void WriteCrashLine(const std::wstring& line) { WriteLine(CrashLogPath(),line); }
}
void ConfigureDiagnosticLogging(bool enabled){g_enabled=enabled;}
bool DiagnosticLoggingEnabled(){return g_enabled;}
void ClearDiagnosticLog(){if(g_enabled)DeleteFileW(LogPath().c_str());}
void DiagnosticLog(const wchar_t* message){if(g_enabled)WriteLine(LogPath(),message?message:L"");}
void DiagnosticLogHRESULT(const wchar_t* operation,HRESULT hr){if(!g_enabled)return;wchar_t text[256];wsprintfW(text,L"%s: HRESULT=0x%08lX",operation,static_cast<unsigned long>(hr));WriteLine(LogPath(),text);}
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* info){
    if(info&&info->ExceptionRecord){
        const EXCEPTION_RECORD* record=info->ExceptionRecord;
        wchar_t text[512];
        wsprintfW(text,L"Unhandled exception: code=0x%08lX address=%p thread=%lu",static_cast<unsigned long>(record->ExceptionCode),record->ExceptionAddress,GetCurrentThreadId());WriteCrashLine(text);
        HMODULE module=nullptr;
        if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(record->ExceptionAddress),&module)){
            wchar_t path[MAX_PATH]{};GetModuleFileNameW(module,path,MAX_PATH);
            swprintf_s(text,L"Faulting module: %s + 0x%llX",path,static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(record->ExceptionAddress)-reinterpret_cast<ULONG_PTR>(module)));WriteCrashLine(text);
        }
        if(record->ExceptionCode==EXCEPTION_ACCESS_VIOLATION&&record->NumberParameters>=2){
            wsprintfW(text,L"Access violation: %s address=%p",record->ExceptionInformation[0]==0?L"read":record->ExceptionInformation[0]==1?L"write":L"execute",reinterpret_cast<void*>(record->ExceptionInformation[1]));WriteCrashLine(text);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
void InstallCrashDiagnostics(){SetUnhandledExceptionFilter(CrashHandler);}
