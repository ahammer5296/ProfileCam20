#pragma once
#include <windows.h>
#include <dshow.h>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include "directshow_compat.h"

struct VideoFormat { int width{}; int height{}; double fps{}; GUID subtype{}; AM_MEDIA_TYPE* mediaType{}; VideoFormat()=default; ~VideoFormat(); VideoFormat(const VideoFormat&)=delete; VideoFormat& operator=(const VideoFormat&)=delete; VideoFormat(VideoFormat&& other) noexcept; VideoFormat& operator=(VideoFormat&& other) noexcept; };
struct VideoDevice { std::wstring friendlyName; std::wstring displayName; std::vector<VideoFormat> formats; };
struct VideoFrame { std::vector<BYTE> pixels; int width{}; int height{}; int stride{}; bool topDown{}; };

class CameraCatalog { public: bool Refresh(); const std::vector<VideoDevice>& Devices() const { return devices_; } private: std::vector<VideoDevice> devices_; static bool EnumerateFormats(VideoDevice&,IMoniker*); };

class CameraCapture { public: CameraCapture()=default; ~CameraCapture(); bool Start(const VideoDevice&,const VideoFormat&,HWND); void Stop(); void AcknowledgeFrame(){notifyPending_.store(false);} bool IsRunning() const { return running_.load(); } bool Snapshot(VideoFrame& frame); const std::wstring& Error() const { return lastError_; } private: class Callback; friend class Callback; bool Fail(const wchar_t*,HRESULT); void OnBuffer(BYTE*,long); void ClearGraph(); IGraphBuilder* graph_{}; ICaptureGraphBuilder2* builder_{}; IBaseFilter* source_{}; IBaseFilter* grabberFilter_{}; IBaseFilter* nullRenderer_{}; ISampleGrabber* grabber_{}; IMediaControl* control_{}; Callback* callback_{}; HWND notifyWindow_{}; std::atomic<bool> running_{}; std::atomic<bool> notifyPending_{}; std::atomic<unsigned> sampleCount_{}; std::mutex frameMutex_; VideoFrame latest_; std::wstring lastError_; };
